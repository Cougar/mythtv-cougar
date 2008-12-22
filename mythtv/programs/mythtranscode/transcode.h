#include "recordingprofile.h"
#include "fifowriter.h"
#include "transcodedefs.h"

class PlayerContext;
class ProgramInfo;
class NuppelVideoRecorder;
class NuppelVideoPlayer;
class RingBuffer;

typedef vector<struct kfatable_entry> KFATable;

class Transcode : public QObject
{
  public:
    Transcode(ProgramInfo *pginfo);
    int TranscodeFile(
        const QString &inputname,
        const QString &outputname,
        const QString &profileName,
        bool honorCutList, bool framecontrol, int jobID,
        QString fifodir, QMap<long long, int> deleteMap);
    void ShowProgress(bool val) { showprogress = val; }
    void SetRecorderOptions(QString options) { recorderOptions = options; }

  private:
    ~Transcode();
    bool GetProfile(QString profileName, QString encodingType, int height,
                    int frameRate);
    void ReencoderAddKFA(long curframe, long lastkey, long num_keyframes);

  private:
    ProgramInfo            *m_proginfo;
    RecordingProfile        profile;
    int                     keyframedist;
    NuppelVideoRecorder    *nvr;
    NuppelVideoPlayer      *nvp;
    PlayerContext          *player_ctx;
    RingBuffer             *inRingBuffer;
    RingBuffer             *outRingBuffer;
    FIFOWriter::FIFOWriter *fifow;
    KFATable               *kfa_table;
    bool                    showprogress;
    QString                 recorderOptions;
};

/* vim: set expandtab tabstop=4 shiftwidth=4: */
