#ifndef REMOTEUTIL_H_
#define REMOTEUTIL_H_

#include <vector>
using namespace std;

class ProgramInfo;
class RemoteEncoder;

vector<ProgramInfo *> *RemoteGetRecordedList(bool deltype);
void RemoteGetFreeSpace(int &totalspace, int &usedspace);
bool RemoteCheckFile(ProgramInfo *pginfo);
void RemoteQueueTranscode(ProgramInfo *pginfo, int state);
void RemoteStopRecording(ProgramInfo *pginfo);
void RemoteDeleteRecording(ProgramInfo *pginfo, bool forgetHistory);
bool RemoteReactivateRecording(ProgramInfo *pginfo);
bool RemoteGetAllPendingRecordings(vector<ProgramInfo *> &recordinglist);
void RemoteGetAllScheduledRecordings(vector<ProgramInfo *> &scheduledlist);
vector<ProgramInfo *> *RemoteGetConflictList(ProgramInfo *pginfo);
void RemoteSendMessage(const QString &message);
RemoteEncoder *RemoteRequestRecorder(void);
RemoteEncoder *RemoteRequestNextFreeRecorder(int curr);
RemoteEncoder *RemoteGetExistingRecorder(ProgramInfo *pginfo);
RemoteEncoder *RemoteGetExistingRecorder(int recordernum);
void RemoteGeneratePreviewPixmap(ProgramInfo *pginfo);
void RemoteFillProginfo(ProgramInfo *pginfo, const QString &playbackhostname);
int RemoteIsRecording(void);
int RemoteGetRecordingMask(void);
int RemoteGetFreeRecorderCount(void);

int RemoteCheckForRecording(ProgramInfo *pginfo);
int RemoteGetRecordingStatus(ProgramInfo *pginfo, int overrecsecs, 
                             int underrecsecs);

#endif
