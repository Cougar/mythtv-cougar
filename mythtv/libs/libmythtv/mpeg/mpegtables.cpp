// -*- Mode: c++ -*-
// Copyright (c) 2003-2004, Daniel Thor Kristjansson
#include "mpegtables.h"
#include "atscdescriptors.h"

const unsigned char init_patheader[9] =
{
    (0x0), (0x0)/*TableID::PAT*/,
    (0xb0), // set one reserved bit???
    (0x0), (0x0), (0x0),
    (0x83), // current | reserved
    (0x0), (0x0)
};

const unsigned char DEFAULT_PMT_HEADER[9] =
{
    0x0, 0x2/*TableID::PMT*/,
    0xb0, // set one reserved bit???
    0x0, 0x0, 0x0,
    0x83, // current | reserved
    0x0, 0x0,
};

ProgramAssociationTable* ProgramAssociationTable::Create(
    uint tsid, uint version,
    const vector<uint>& pnum, const vector<uint>& pid)
{
    const uint count = min(pnum.size(), pid.size());
    ProgramAssociationTable* pat = CreateBlank();
    pat->SetVersionNumber(version);
    pat->SetTranportStreamID(tsid);
    pat->SetLength(PSIP_OFFSET+(count*4));

    // create PAT data
    if ((count * 4) >= (184 - PSIP_OFFSET))
    { // old PAT must be in single TS for this create function
        VERBOSE(VB_IMPORTANT, "PAT::Create: Error, old "
                "PAT size exceeds maximum PAT size.");
        return 0;
    }

    uint offset = PSIP_OFFSET;
    for (uint i=0; i<count; i++)
    {
        // pnum
        pat->pesdata()[offset++] = pnum[i]>>8;
        pat->pesdata()[offset++] = pnum[i] & 0xff;
        // pid
        pat->pesdata()[offset++] = ((pid[i]>>8) & 0x1f) | 0xe0;
        pat->pesdata()[offset++] = pid[i] & 0xff;
    }

    pat->Finalize();

    return pat;
}

ProgramMapTable* ProgramMapTable::Create(
    uint programNumber, uint basepid, uint pcrpid, uint version,
    vector<uint> pids, vector<uint> types)
{
    const uint count = min(pids.size(), types.size());
    ProgramMapTable* pmt = CreateBlank();
    pmt->tsheader()->SetPID(basepid);

    pmt->RemoveAllStreams();
    pmt->SetProgramNumber(programNumber);
    pmt->SetPCRPID(pcrpid);
    pmt->SetVersionNumber(version);

    for (uint i=0; i<count; i++)
        pmt->AppendStream(pids[i], types[i]);
    pmt->Finalize();

    return pmt;
}

void  ProgramMapTable::Parse() const
{
    _ptrs.clear();
    unsigned char *pos =
      const_cast<unsigned char*>
        (pesdata() + PSIP_OFFSET + pmt_header + ProgramInfoLength());
    for (unsigned int i=0; pos < pesdata()+Length(); i++) {
        _ptrs.push_back(pos);
        pos += 5 + StreamInfoLength(i);
    }
    _ptrs.push_back(pos);
}

void ProgramMapTable::AppendStream(
    unsigned int pid, unsigned int type,
    unsigned char* streamInfo, unsigned int infoLength)
{
    if (!StreamCount())
        _ptrs.push_back(pesdata() + PSIP_OFFSET +
                        pmt_header + ProgramInfoLength());
    memset(_ptrs[StreamCount()], 0xff, 5);
    SetStreamPID(StreamCount(), pid);
    SetStreamType(StreamCount(), type);
    SetStreamProgramInfo(StreamCount(), streamInfo, infoLength);
    _ptrs.push_back(_ptrs[StreamCount()]+5+StreamInfoLength(StreamCount()));
    SetLength(_ptrs[StreamCount()] - pesdata());
}

const QString PSIPTable::toString() const
{
    QString str;
    //for (unsigned int i=0; i<9; i++)
    //str.append(QString(" 0x%1").arg(int(pesdata()[i]), 0, 16));
    //str.append("\n");
    str.append(QString(" PSIP prefix(0x%1) tableID(0x%1) length(%2) extension(0x%3)\n").
               arg(StartCodePrefix(), 0, 16).arg(TableID(), 0, 16).
               arg(Length()).arg(TableIDExtension(), 0, 16));
    str.append(QString("      version(%1) current(%2) section(%3) last_section(%4)\n").
               arg(Version()).arg(IsCurrent()).arg(Section()).arg(LastSection()));
    //str.append(QString("   protocol ver: "<<protocolVersion()<<endl;
    return str;
}

const QString ProgramAssociationTable::toString() const
{
    QString str;
    str.append(QString("Program Association Table\n"));
    str.append(static_cast<const PSIPTable*>(this)->toString());
    str.append(QString("         tsid: %1\n").arg(TransportStreamID()));
    str.append(QString(" programCount: %1\n").arg(ProgramCount()));
    for (unsigned int i=0; i<ProgramCount(); i++) {
        const unsigned char* p=pesdata()+PSIP_OFFSET+(i<<2);
        str.append(QString("  program number %1").arg(int(ProgramNumber(i)))).
            append(QString(" has PID 0x%1   data ").arg(ProgramPID(i),4,16)).
            append(QString(" 0x%1 0x%2").arg(int(p[0])).arg(int(p[1]))).
            append(QString(" 0x%1 0x%2\n").arg(int(p[2])).arg(int(p[3])));
    }
    return str;
}

const QString ProgramMapTable::toString() const
{
    Parse();
    QString str;
    str.append(QString("Program Map Table ver(%1) pid(0x%2) pnum(%3)\n").
               arg(Version()).arg(tsheader()->PID(), 0, 16).arg(ProgramNumber()));
    if (0!=ProgramInfoLength()) {
        vector<const unsigned char*> desc = 
            ATSCDescriptor::Parse(ProgramInfo(), ProgramInfoLength());
        for (uint i=0; i<desc.size(); i++)
            str.append(QString("  %1\n").arg(ATSCDescriptor(desc[i]).toString()));
    }
    str.append("\n");
    for (unsigned int i=0; i<StreamCount(); i++) {
        str.append(QString(" Stream #%1 pid(0x%2) type(%3  0x%4)\n").
                   arg(i).arg(StreamPID(i), 0, 16).
                   arg(StreamTypeString(i)).arg(int(StreamType(i))));
        if (0!=StreamInfoLength(i)) {
            vector<const unsigned char*> desc = 
                ATSCDescriptor::Parse(StreamInfo(i), StreamInfoLength(i));
            for (uint i=0; i<desc.size(); i++)
                str.append(QString("  %1\n").arg(ATSCDescriptor(desc[i]).toString()));
        }
    }
    return str;
}

const char *nameStream(unsigned int streamID)
{
    // valid for some ATSC stuff too
    char* retval = "unknown";
    if (StreamID::MPEG2Video==streamID) return "video-mp2";
    else if (StreamID::MPEG1Video==streamID) return "video-mp1";
    else if (StreamID::AC3Audio==streamID)
        return "audio-ac3";  // EIT, PMT
    else if (StreamID::MPEG2Audio==streamID)
        return "audio-mp2-layer[1,2,3]"; // EIT, PMT
    else if (StreamID::MPEG1Audio==streamID)
        return "audio-mp1-layer[1,2,3]"; // EIT, PMT
    else if (StreamID::AACAudio==streamID)
        return "audio-aac"; // EIT, PMT
    else if (StreamID::DTSAudio==streamID)
        return "audio-dts"; // EIT, PMT
    else switch (streamID) {
        case (TableID::STUFFING):
            retval="stuffing"; break; // optionally in any
        case (TableID::CAPTION):
            retval="caption service"; break; // EIT, optionally in PMT
        case (TableID::CENSOR):
            retval="censor"; break; // EIT, optionally in PMT
        case (TableID::ECN):
            retval="extended channel name"; break;
        case (TableID::SRVLOC):
            retval="service location"; break; // required in VCT
        case (TableID::TSS): // other channels with same stuff
            retval="time-shifted service"; break;
        case (TableID::CMPNAME): retval="component name"; break; //??? PMT
    }
    return retval;
}

const QString ProgramMapTable::StreamTypeString(unsigned int i) const
{
    return QString( nameStream(StreamType(i)) );
}

