// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#ifndef _PES_PACKET_H_
#define _PES_PACKET_H_

/*
  max length of PSI table = 1024 bytes
  max length of private_section = 4096 bytes
*/

#include "tspacket.h"
extern "C" {
#include "../libavcodec/avcodec.h"
#include "../libavformat/avformat.h"
#include "../libavformat/mpegts.h"
}

inline unsigned int pes_length(const unsigned char* pesbuf) {
    return (pesbuf[2] & 0x0f) << 8 | pesbuf[3];
}

class PESPacket {
    // only handles single TS packet PES packets, for tables basically
    void InitPESPacket(TSPacket& tspacket) {
        if (tspacket.PayloadStart()) {
            _pesdata = tspacket.data() + tspacket.AFCOffset() + tspacket.StartOfFieldPointer();
        } else {
            cerr<<"Started PESPacket, but !payloadStart()"<<endl;
            _pesdata = tspacket.data() + tspacket.AFCOffset();
        }

        _badPacket=true;
        if ((_pesdata - tspacket.data())<(188-3-4)) {
            _badPacket = CalcCRC()!=CRC();
        }
    }

  protected:
    // does not create it's own data
    PESPacket(const TSPacket* tspacket, bool) : _pesdata(0), _fullbuffer(0), _allocSize(0)
    {
        InitPESPacket(const_cast<TSPacket&>(*tspacket));
        _fullbuffer = const_cast<unsigned char*>(tspacket->data());
    }
  private:
    //const PESPacket& operator=(const PESPacket& pkt);
    PESPacket& operator=(const PESPacket& pkt);
  public:
    // may be modified
    PESPacket(const PESPacket& pkt)
    { // clone
        _allocSize = pkt._allocSize;
        _fullbuffer = new unsigned char[_allocSize+188];
        memcpy(_fullbuffer, pkt._fullbuffer, _allocSize+188);
        _pesdata = _fullbuffer + (pkt._pesdata - pkt._fullbuffer);
        _ccLast = pkt._ccLast;
        _cnt = pkt._cnt;
        _badPacket = pkt._badPacket;
    }

    // may be modified
    PESPacket(const TSPacket& tspacket)
    { // clone
        InitPESPacket(const_cast<TSPacket&>(tspacket));

        int len = (4*1024) - 188;
        int offset = _pesdata - tspacket.data();
        _allocSize=((len+offset)/188)*188+188;
        _fullbuffer = new unsigned char[_allocSize+188];
        _pesdata = _fullbuffer + offset;
        memcpy(_fullbuffer, tspacket.data(), 188);
        _ccLast = tspacket.ContinuityCounter();
        _cnt = 1;
    }

    virtual ~PESPacket()
    {
        if (IsClone())
            delete[] _fullbuffer;
        _fullbuffer = 0;
        _pesdata = 0;
    }

    static const PESPacket View(const TSPacket& tspacket) {
        return PESPacket(&tspacket, false);
    }

    static PESPacket View(TSPacket& tspacket) {
        return PESPacket(&tspacket, false);
    }

    bool IsClone() const { return bool(_allocSize); }

    // return true if complete or broken
    bool AddTSPacket(const TSPacket* tspacket);

    bool IsGood() const { return !_badPacket; }

    const TSHeader* tsheader() const 
    {
        return reinterpret_cast<const TSHeader*>(_fullbuffer);
    }

    TSHeader* tsheader() { return reinterpret_cast<TSHeader*>(_fullbuffer); }

    unsigned int StartCodePrefix() const { return _pesdata[0]; }
    unsigned int StreamID() const { return _pesdata[1]; }
    unsigned int Length() const {
        return (pesdata()[2] & 0x0f) << 8 | pesdata()[3];
    }

    const unsigned char* pesdata() const { return _pesdata; }
    unsigned char* pesdata() { return _pesdata; }

    void SetStartCodePrefix(unsigned int val) { _pesdata[0]=val&0xff; }
    void SetStreamID(unsigned int id) { _pesdata[1]=id; }
    void SetLength(unsigned int len) {
        pesdata()[2] = (pesdata()[2] & 0xf0) | ((len>>8) & 0x0f);
        pesdata()[3] = len & 0xff;
    }

    unsigned int CRC() const {
        unsigned int offset = Length();
        return ((_pesdata[offset+0]<<24) |
                (_pesdata[offset+1]<<16) |
                (_pesdata[offset+2]<<8) |
                (_pesdata[offset+3]));
    }
    unsigned int CalcCRC() const {
        return mpegts_crc32(_pesdata+1, Length()-1);
    }
    void SetCRC(unsigned int crc) {
        _pesdata[Length()+0] = (crc & 0xff000000) >> 24;
        _pesdata[Length()+1] = (crc & 0x00ff0000) >> 16;
        _pesdata[Length()+2] = (crc & 0x0000ff00) >> 8;
        _pesdata[Length()+3] = (crc & 0x000000ff);
    }
    bool VerifyCRC() const { return CalcCRC()==CRC(); }

    /*
    // for testing
    unsigned int t_CRC32(int offset) const {
        return ((_pesdata[offset+0]<<24) |
                (_pesdata[offset+1]<<16) |
                (_pesdata[offset+2]<<8) |
                (_pesdata[offset+3]));
    }
    // for testing
    unsigned int t_calcCRC32(int offset) const {
        return mpegts_crc32(_pesdata+1, offset-1);
    }
    */
  protected:
    void Finalize() { SetCRC(CalcCRC()); }

    unsigned char *_pesdata;
    unsigned char *_fullbuffer;
  private:
    unsigned int _ccLast;
    unsigned int _cnt;
    unsigned int _allocSize;
    bool _badPacket;
};

#endif // _PES_PACKET_H_
