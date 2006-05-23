////////////////////////////////////////////////////////////////////////////////
/// 
/// Sample rate transposer. Changes sample rate by using linear interpolation 
/// together with anti-alias filtering (first order interpolation with anti-
/// alias filtering should be quite adequate for this application)
///
/// Author        : Copyright (c) Olli Parviainen
/// Author e-mail : oparviai 'at' iki.fi
/// SoundTouch WWW: http://www.surina.net/soundtouch
///
////////////////////////////////////////////////////////////////////////////////
//
// Last changed  : $Date$
// File revision : $Revision$
//
// $Id$
//
////////////////////////////////////////////////////////////////////////////////
//
// License :
//
//  SoundTouch audio processing library
//  Copyright (c) Olli Parviainen
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with this library; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
////////////////////////////////////////////////////////////////////////////////

#include <memory.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include "RateTransposer.h"
#include "AAFilter.h"

using namespace soundtouch;


/// A linear samplerate transposer class that uses integer arithmetics.
/// for the transposing.
class RateTransposerInteger : public RateTransposer
{
protected:
    int iSlopeCount;
    uint uRate;
    SAMPLETYPE sPrevSampleL, sPrevSampleR;

    virtual void resetRegisters();

    virtual uint transposeStereo(SAMPLETYPE *dest, 
                         const SAMPLETYPE *src, 
                         uint numSamples);
    virtual uint transposeMono(SAMPLETYPE *dest, 
                       const SAMPLETYPE *src, 
                       uint numSamples);

public:
    RateTransposerInteger();
    virtual ~RateTransposerInteger();

    /// Sets new target rate. Normal rate = 1.0, smaller values represent slower 
    /// rate, larger faster rates.
    virtual void setRate(float newRate);

};


/// A linear samplerate transposer class that uses floating point arithmetics
/// for the transposing.
class RateTransposerFloat : public RateTransposer
{
protected:
    float fSlopeCount;
    float fRateStep;
    SAMPLETYPE sPrevSampleL, sPrevSampleR;

    virtual void resetRegisters();

    virtual uint transposeStereo(SAMPLETYPE *dest, 
                         const SAMPLETYPE *src, 
                         uint numSamples);
    virtual uint transposeMono(SAMPLETYPE *dest, 
                       const SAMPLETYPE *src, 
                       uint numSamples);

public:
    RateTransposerFloat();
    virtual ~RateTransposerFloat();
};



#ifndef min
#define min(a,b) ((a > b) ? b : a)
#define max(a,b) ((a < b) ? b : a)
#endif


// Operator 'new' is overloaded so that it automatically creates a suitable instance 
// depending on if we've a MMX/SSE/etc-capable CPU available or not.
void * RateTransposer::operator new(size_t s)
{
    // Notice! don't use "new TDStretch" directly, use "newInstance" to create a new instance instead!
    assert(FALSE);  
    return NULL;
}


RateTransposer *RateTransposer::newInstance()
{
#ifdef INTEGER_SAMPLES
    return ::new RateTransposerInteger;
#else
    return ::new RateTransposerFloat;
#endif
}


// Constructor
RateTransposer::RateTransposer() : FIFOProcessor(&outputBuffer)
{
    uChannels = 2;
    bUseAAFilter = TRUE;

    // Instantiates the anti-alias filter with default tap length
    // of 32
    pAAFilter = new AAFilter(32);
}



RateTransposer::~RateTransposer()
{
    delete pAAFilter;
}



/// Enables/disables the anti-alias filter. Zero to disable, nonzero to enable
void RateTransposer::enableAAFilter(const BOOL newMode)
{
    bUseAAFilter = newMode;
}


/// Returns nonzero if anti-alias filter is enabled.
BOOL RateTransposer::isAAFilterEnabled() const
{
    return bUseAAFilter;
}


AAFilter *RateTransposer::getAAFilter() const
{
    return pAAFilter;
}



// Sets new target uRate. Normal uRate = 1.0, smaller values represent slower 
// uRate, larger faster uRates.
void RateTransposer::setRate(float newRate)
{
    float fCutoff;

    fRate = newRate;

    // design a new anti-alias filter
    if (newRate > 1.0f) 
    {
        fCutoff = 0.5f / newRate;
    } 
    else 
    {
        fCutoff = 0.5f * newRate;
    }
    pAAFilter->setCutoffFreq(fCutoff);
}


// Outputs as many samples of the 'outputBuffer' as possible, and if there's
// any room left, outputs also as many of the incoming samples as possible.
// The goal is to drive the outputBuffer empty.
//
// It's allowed for 'output' and 'input' parameters to point to the same
// memory position.
void RateTransposer::flushStoreBuffer()
{
    if (storeBuffer.isEmpty()) return;

    outputBuffer.moveSamples(storeBuffer);
}


// Adds 'numSamples' pcs of samples from the 'samples' memory position into
// the input of the object.
void RateTransposer::putSamples(const SAMPLETYPE *samples, uint numSamples)
{
    processSamples(samples, numSamples);
}



// Transposes up the sample rate, causing the observed playback 'rate' of the
// sound to decrease
void RateTransposer::upsample(const SAMPLETYPE *src, uint numSamples)
{
    int count, sizeTemp, num;

    // If the parameter 'uRate' value is smaller than 'SCALE', first transpose
    // the samples and then apply the anti-alias filter to remove aliasing.

    // First check that there's enough room in 'storeBuffer' 
    // (+16 is to reserve some slack in the destination buffer)
    sizeTemp = (int)((float)numSamples / fRate + 16.0f);

    // Transpose the samples, store the result into the end of "storeBuffer"
    count = transpose(storeBuffer.ptrEnd(sizeTemp), src, numSamples);
    storeBuffer.putSamples(count);

    // Apply the anti-alias filter to samples in "store output", output the
    // result to "dest"
    num = storeBuffer.numSamples();
    count = pAAFilter->evaluate(outputBuffer.ptrEnd(num), 
        storeBuffer.ptrBegin(), num, uChannels);
    outputBuffer.putSamples(count);

    // Remove the processed samples from "storeBuffer"
    storeBuffer.receiveSamples(count);
}


// Transposes down the sample rate, causing the observed playback 'rate' of the
// sound to increase
void RateTransposer::downsample(const SAMPLETYPE *src, uint numSamples)
{
    int count, sizeTemp;

    // If the parameter 'uRate' value is larger than 'SCALE', first apply the
    // anti-alias filter to remove high frequencies (prevent them from folding
    // over the lover frequencies), then transpose. */

    // Add the new samples to the end of the storeBuffer */
    storeBuffer.putSamples(src, numSamples);

    // Anti-alias filter the samples to prevent folding and output the filtered 
    // data to tempBuffer. Note : because of the FIR filter length, the
    // filtering routine takes in 'filter_length' more samples than it outputs.
    assert(tempBuffer.isEmpty());
    sizeTemp = storeBuffer.numSamples();

    count = pAAFilter->evaluate(tempBuffer.ptrEnd(sizeTemp), 
        storeBuffer.ptrBegin(), sizeTemp, uChannels);

    // Remove the filtered samples from 'storeBuffer'
    storeBuffer.receiveSamples(count);

    // Transpose the samples (+16 is to reserve some slack in the destination buffer)
    sizeTemp = (int)((float)numSamples / fRate + 16.0f);
    count = transpose(outputBuffer.ptrEnd(sizeTemp), tempBuffer.ptrBegin(), count);
    outputBuffer.putSamples(count);
}


// Transposes sample rate by applying anti-alias filter to prevent folding. 
// Returns amount of samples returned in the "dest" buffer.
// The maximum amount of samples that can be returned at a time is set by
// the 'set_returnBuffer_size' function.
void RateTransposer::processSamples(const SAMPLETYPE *src, uint numSamples)
{
    uint count;
    uint sizeReq;

    if (numSamples == 0) return;
    assert(pAAFilter);

    // If anti-alias filter is turned off, simply transpose without applying
    // the filter
    if (bUseAAFilter == FALSE) 
    {
        sizeReq = (int)((float)numSamples / fRate + 1.0f);
        count = transpose(outputBuffer.ptrEnd(sizeReq), src, numSamples);
        outputBuffer.putSamples(count);
        return;
    }

    // Transpose with anti-alias filter
    if (fRate < 1.0f) 
    {
        upsample(src, numSamples);
    } 
    else  
    {
        downsample(src, numSamples);
    }
}


// Transposes the sample rate of the given samples using linear interpolation. 
// Returns the number of samples returned in the "dest" buffer
inline uint RateTransposer::transpose(SAMPLETYPE *dest, const SAMPLETYPE *src, uint numSamples)
{
    if (uChannels == 2) 
    {
        return transposeStereo(dest, src, numSamples);
    } 
    else 
    {
        return transposeMono(dest, src, numSamples);
    }
}


// Sets the number of channels, 1 = mono, 2 = stereo
void RateTransposer::setChannels(const uint numchannels)
{
    if (uChannels == numchannels) return;

    assert(numchannels == 1 || numchannels == 2);
    uChannels = numchannels;

    storeBuffer.setChannels(uChannels);
    tempBuffer.setChannels(uChannels);
    outputBuffer.setChannels(uChannels);

    // Inits the linear interpolation registers
    resetRegisters();
}


// Clears all the samples in the object
void RateTransposer::clear()
{
    outputBuffer.clear();
    storeBuffer.clear();
}


// Returns nonzero if there aren't any samples available for outputting.
uint RateTransposer::isEmpty()
{
    int res;

    res = FIFOProcessor::isEmpty();
    if (res == 0) return 0;
    return storeBuffer.isEmpty();
}


//////////////////////////////////////////////////////////////////////////////
//
// RateTransposerInteger - integer arithmetic implementation
// 

/// fixed-point interpolation routine precision
#define SCALE    65536

// Constructor
RateTransposerInteger::RateTransposerInteger() : RateTransposer()
{
    // call these here as these are virtual functions; calling these
    // from the base class constructor wouldn't execute the overloaded
    // versions (<master yoda>peculiar C++ can be</my>).
    resetRegisters();
    setRate(1.0f);
}


RateTransposerInteger::~RateTransposerInteger()
{
}


void RateTransposerInteger::resetRegisters()
{
    iSlopeCount = 0;
    sPrevSampleL = 
    sPrevSampleR = 0;
}



// Transposes the sample rate of the given samples using linear interpolation. 
// 'Mono' version of the routine. Returns the number of samples returned in 
// the "dest" buffer
uint RateTransposerInteger::transposeMono(SAMPLETYPE *dest, const SAMPLETYPE *src, uint numSamples)
{
    unsigned int i, used;
    LONG_SAMPLETYPE temp, vol1;

    used = 0;    
    i = 0;

    // Process the last sample saved from the previous call first...
    while (iSlopeCount <= SCALE) 
    {
        vol1 = (LONG_SAMPLETYPE)(SCALE - iSlopeCount);
        temp = vol1 * sPrevSampleL + iSlopeCount * src[0];
        dest[i] = (SAMPLETYPE)(temp / SCALE);
        i++;
        iSlopeCount += uRate;
    }
    // now always (iSlopeCount > SCALE)
    iSlopeCount -= SCALE;

    while (1)
    {
        while (iSlopeCount > SCALE) 
        {
            iSlopeCount -= SCALE;
            used ++;
            if (used >= numSamples - 1) goto end;
        }
        vol1 = (LONG_SAMPLETYPE)(SCALE - iSlopeCount);
        temp = src[used] * vol1 + iSlopeCount * src[used + 1];
        dest[i] = (SAMPLETYPE)(temp / SCALE);

        i++;
        iSlopeCount += uRate;
    }
end:
    // Store the last sample for the next round
    sPrevSampleL = src[numSamples - 1];

    return i;
}


// Transposes the sample rate of the given samples using linear interpolation. 
// 'Stereo' version of the routine. Returns the number of samples returned in 
// the "dest" buffer
uint RateTransposerInteger::transposeStereo(SAMPLETYPE *dest, const SAMPLETYPE *src, uint numSamples)
{
    unsigned int srcPos, i, used;
    LONG_SAMPLETYPE temp, vol1;

    if (numSamples == 0) return 0;  // no samples, no work

    used = 0;    
    i = 0;

    // Process the last sample saved from the sPrevSampleLious call first...
    while (iSlopeCount <= SCALE) 
    {
        vol1 = (LONG_SAMPLETYPE)(SCALE - iSlopeCount);
        temp = vol1 * sPrevSampleL + iSlopeCount * src[0];
        dest[2 * i] = (SAMPLETYPE)(temp / SCALE);
        temp = vol1 * sPrevSampleR + iSlopeCount * src[1];
        dest[2 * i + 1] = (SAMPLETYPE)(temp / SCALE);
        i++;
        iSlopeCount += uRate;
    }
    // now always (iSlopeCount > SCALE)
    iSlopeCount -= SCALE;

    while (1)
    {
        while (iSlopeCount > SCALE) 
        {
            iSlopeCount -= SCALE;
            used ++;
            if (used >= numSamples - 1) goto end;
        }
        srcPos = 2 * used;
        vol1 = (LONG_SAMPLETYPE)(SCALE - iSlopeCount);
        temp = src[srcPos] * vol1 + iSlopeCount * src[srcPos + 2];
        dest[2 * i] = (SAMPLETYPE)(temp / SCALE);
        temp = src[srcPos + 1] * vol1 + iSlopeCount * src[srcPos + 3];
        dest[2 * i + 1] = (SAMPLETYPE)(temp / SCALE);

        i++;
        iSlopeCount += uRate;
    }
end:
    // Store the last sample for the next round
    sPrevSampleL = src[2 * numSamples - 2];
    sPrevSampleR = src[2 * numSamples - 1];

    return i;
}


// Sets new target uRate. Normal uRate = 1.0, smaller values represent slower 
// uRate, larger faster uRates.
void RateTransposerInteger::setRate(float newRate)
{
    uRate = (int)(newRate * SCALE + 0.5f);
    RateTransposer::setRate(newRate);
}


//////////////////////////////////////////////////////////////////////////////
//
// RateTransposerFloat - floating point arithmetic implementation
// 
//////////////////////////////////////////////////////////////////////////////

// Constructor
RateTransposerFloat::RateTransposerFloat() : RateTransposer()
{
    // call these here as these are virtual functions; calling these
    // from the base class constructor wouldn't execute the overloaded
    // versions (<master yoda>peculiar C++ can be</my>).
    resetRegisters();
    setRate(1.0f);
}


RateTransposerFloat::~RateTransposerFloat()
{
}


void RateTransposerFloat::resetRegisters()
{
    fSlopeCount = 0;
    sPrevSampleL = 
    sPrevSampleR = 0;
}



// Transposes the sample rate of the given samples using linear interpolation. 
// 'Mono' version of the routine. Returns the number of samples returned in 
// the "dest" buffer
uint RateTransposerFloat::transposeMono(SAMPLETYPE *dest, const SAMPLETYPE *src, uint numSamples)
{
    unsigned int i, used;

    used = 0;    
    i = 0;

    // Process the last sample saved from the previous call first...
    while (fSlopeCount <= 1.0f) 
    {
        dest[i] = (SAMPLETYPE)((1.0f - fSlopeCount) * sPrevSampleL + fSlopeCount * src[0]);
        i++;
        fSlopeCount += fRate;
    }
    fSlopeCount -= 1.0f;

    if (numSamples == 1) goto end;

    while (1)
    {
        while (fSlopeCount > 1.0f) 
        {
            fSlopeCount -= 1.0f;
            used ++;
            if (used >= numSamples - 1) goto end;
        }
        dest[i] = (SAMPLETYPE)((1.0f - fSlopeCount) * src[used] + fSlopeCount * src[used + 1]);
        i++;
        fSlopeCount += fRate;
    }
end:
    // Store the last sample for the next round
    sPrevSampleL = src[numSamples - 1];

    return i;
}


// Transposes the sample rate of the given samples using linear interpolation. 
// 'Mono' version of the routine. Returns the number of samples returned in 
// the "dest" buffer
uint RateTransposerFloat::transposeStereo(SAMPLETYPE *dest, const SAMPLETYPE *src, uint numSamples)
{
    unsigned int srcPos, i, used;

    if (numSamples == 0) return 0;  // no samples, no work

    used = 0;    
    i = 0;

    // Process the last sample saved from the sPrevSampleLious call first...
    while (fSlopeCount <= 1.0f) 
    {
        dest[2 * i] = (SAMPLETYPE)((1.0f - fSlopeCount) * sPrevSampleL + fSlopeCount * src[0]);
        dest[2 * i + 1] = (SAMPLETYPE)((1.0f - fSlopeCount) * sPrevSampleR + fSlopeCount * src[1]);
        i++;
        fSlopeCount += fRate;
    }
    // now always (iSlopeCount > 1.0f)
    fSlopeCount -= 1.0f;

    if (numSamples == 1) goto end;

    while (1)
    {
        while (fSlopeCount > 1.0f) 
        {
            fSlopeCount -= 1.0f;
            used ++;
            if (used >= numSamples - 1) goto end;
        }
        srcPos = 2 * used;

        dest[2 * i] = (SAMPLETYPE)((1.0f - fSlopeCount) * src[srcPos] 
            + fSlopeCount * src[srcPos + 2]);
        dest[2 * i + 1] = (SAMPLETYPE)((1.0f - fSlopeCount) * src[srcPos + 1] 
            + fSlopeCount * src[srcPos + 3]);

        i++;
        fSlopeCount += fRate;
    }
end:
    // Store the last sample for the next round
    sPrevSampleL = src[2 * numSamples - 2];
    sPrevSampleR = src[2 * numSamples - 1];

    return i;
}
