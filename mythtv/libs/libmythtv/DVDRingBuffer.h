// -*- Mode: c++ -*-
#ifndef DVD_RING_BUFFER_H_
#define DVD_RING_BUFFER_H_

#define DVD_BLOCK_SIZE 2048LL
#define DVD_MENU_MAX 7

#include <QMap>
#include <QString>
#include <QMutex>

#include "util.h"
extern "C" {
#include "avcodec.h"
}

#include "dvdnav/dvdnav.h"

/** \class DVDRingBufferPriv
 *  \brief RingBuffer class for DVD's
 *
 *   A spiffy little class to allow a RingBuffer to read from DVDs.
 */

class NuppelVideoPlayer;

class MPUBLIC DVDRingBufferPriv
{
  public:
    DVDRingBufferPriv();
    virtual ~DVDRingBufferPriv();

    // gets
    int  GetTitle(void) const { return m_title;        }
    int  GetPart(void)  const { return m_part;         }
    bool IsInMenu(void) const;
    bool IsOpen(void)   const { return m_dvdnav;       }
    long long GetReadPosition(void);
    long long GetTotalReadPosition(void) { return m_titleLength; }
    void GetDescForPos(QString &desc) const;
    void GetPartAndTitle(int &_part, int &_title) const
        { _part  = m_part; _title = m_title; }
    uint GetTotalTimeOfTitle(void);
    uint GetChapterLength(void) const { return m_pgLength / 90000; }
    uint GetCellStart(void);
    bool PGCLengthChanged(void);
    bool CellChanged(void);
    bool InStillFrame(void) const { return m_cellHasStillFrame; }
    bool AudioStreamsChanged(void) const { return m_audioStreamsChanged; }
    bool IsWaiting(void) const { return m_dvdWaiting; }
    int  NumPartsInTitle(void) const { return m_titleParts; }
    void GetMenuSPUPkt(uint8_t *buf, int len, int stream_id);

    QRect GetButtonCoords(void);
    AVSubtitle *GetMenuSubtitle(void);
    void ReleaseMenuButton(void);

    bool IgnoringStillorWait(void) { return m_skipstillorwait; }
    uint GetAudioLanguage(int id);
    uint GetSubtitleLanguage(int key);
    void SetMenuPktPts(long long pts) { m_menupktpts = pts; }
    long long GetMenuPktPts(void) { return m_menupktpts; }
    bool DecodeSubtitles(AVSubtitle * sub, int * gotSubtitles,
                         const uint8_t * buf, int buf_size);
    bool GetNameAndSerialNum(QString& _name, QString& _serialnum);
    bool JumpToTitle(void) { return m_jumptotitle; }
    double GetFrameRate(void);
    bool StartOfTitle(void) { return (m_part == 0); }
    bool EndOfTitle(void)   { return    ((!m_titleParts) ||
                                        (m_part == (m_titleParts - 1)) ||
                                        (m_titleParts == 1)); }
    int GetCellID(void) { return m_cellid; }
    int GetVobID(void)  { return m_vobid; }
    bool IsSameChapter(int tmpcellid, int tmpvobid);
    void RunSeekCellStart(void);

    // commands
    bool OpenFile(const QString &filename);
    void PlayTitleAndPart(int _title, int _part)
        { dvdnav_part_play(m_dvdnav, _title, _part); }
    void CloseDVD(void);
    bool nextTrack(void);
    void prevTrack(void);
    int  safe_read(void *data, unsigned sz);
    long long NormalSeek(long long time);
    void SkipStillFrame(void);
    void WaitSkip(void);
    bool GoToMenu(const QString str);
    void GoToNextProgram(void);
    void GoToPreviousProgram(void);
    void MoveButtonLeft(void);
    void MoveButtonRight(void);
    void MoveButtonUp(void);
    void MoveButtonDown(void);
    void ActivateButton(void);
    int NumMenuButtons(void) const;
    void IgnoreStillOrWait(bool skip) { m_skipstillorwait = skip; }
    void InStillFrame(bool change);
    void AudioStreamsChanged(bool change) { m_audioStreamsChanged = change; }
    uint GetCurrentTime(void) { return (m_currentTime / 90000); }
    uint TitleTimeLeft(void);
    void  SetTrack(uint type, int trackNo);
    int   GetTrack(uint type);
    uint8_t GetNumAudioChannels(int id);
    void JumpToTitle(bool change) { m_jumptotitle = change; }
    void SetDVDSpeed(void);
    void SetDVDSpeed(int speed);
    void SetRunSeekCellStart(bool change) { m_runSeekCellStart = change; }

    void SetParent(NuppelVideoPlayer *p) { m_parent = p; }


  protected:
    dvdnav_t      *m_dvdnav;
    unsigned char  m_dvdBlockWriteBuf[DVD_BLOCK_SIZE];
    unsigned char *m_dvdBlockReadBuf;
    QString        m_dvdFilename;
    int            m_dvdBlockRPos;
    int            m_dvdBlockWPos;
    long long      m_pgLength;
    long long      m_pgcLength;
    long long      m_cellStart;
    bool           m_cellChanged;
    bool           m_pgcLengthChanged;
    long long      m_pgStart;
    long long      m_currentpos;
    dvdnav_t      *m_lastNav; // This really belongs in the player.
    int32_t        m_part;
    int32_t        m_title;
    int32_t        m_titleParts;
    bool           m_gotStop;

    bool           m_cellHasStillFrame;
    bool           m_audioStreamsChanged;
    bool           m_dvdWaiting;
    long long      m_titleLength;
    MythTimer      m_stillFrameTimer;
    uint32_t       m_clut[16];
    uint8_t        m_button_color[4];
    uint8_t        m_button_alpha[4];
    QRect          m_hl_button;
    uint8_t       *m_menuSpuPkt;
    int            m_menuBuflength;
    AVSubtitle     m_dvdMenuButton;
    bool           m_skipstillorwait;
    long long      m_cellstartPos;
    bool           m_buttonSelected;
    bool           m_buttonExists;
    int            m_cellid;
    int            m_lastcellid;
    int            m_vobid;
    int            m_lastvobid;
    bool           m_cellRepeated;
    int            m_buttonstreamid;
    bool           m_runningCellStart;
    bool           m_runSeekCellStart;
    long long      m_menupktpts;
    int            m_curAudioTrack;
    int8_t         m_curSubtitleTrack;
    bool           m_autoselectaudio;
    bool           m_autoselectsubtitle;
    bool           m_jumptotitle;
    long long      m_seekpos;
    int            m_seekwhence;
    const char    *m_dvdname;
    const char    *m_serialnumber;
    bool           m_seeking;
    uint64_t       m_seektime;
    uint           m_currentTime;
    QMap<uint, uint> m_seekSpeedMap;
//    QMap<uint, uint> m_audioTrackMap;
//    QMap<uint, uint> m_subTrackMap;

    NuppelVideoPlayer *m_parent;

    QMutex m_menuBtnLock;
    QMutex m_seekLock;

    long long Seek(long long time);
    bool DVDButtonUpdate(bool b_mode);
    void ClearMenuSPUParameters(void);
    void ClearMenuButton(void);
    bool MenuButtonChanged(void);
    uint ConvertLangCode(uint16_t code);
    void SelectDefaultButton(void);
    void ClearSubtitlesOSD(void);
    bool SeekCellStart(void);

    int get_nibble(const uint8_t *buf, int nibble_offset);
    int decode_rle(uint8_t *bitmap, int linesize, int w, int h,
                    const uint8_t *buf, int nibble_offset, int buf_size);
    void guess_palette(uint32_t *rgba_palette,uint8_t *palette,
                        uint8_t *alpha);
    int is_transp(const uint8_t *buf, int pitch, int n,
                  const uint8_t *transp_color);
    int find_smallest_bounding_rectangle(AVSubtitle *s);
};

#endif // DVD_RING_BUFFER_H_
