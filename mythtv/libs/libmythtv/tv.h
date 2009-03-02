#ifndef TV_H
#define TV_H

#include <qstring.h>
#include "videoouttypes.h"

/** \brief ChannelChangeDirection is an enumeration of possible channel
 *         changing directions.
 */
typedef enum
{
    CHANNEL_DIRECTION_UP       = 0,
    CHANNEL_DIRECTION_DOWN     = 1,
    CHANNEL_DIRECTION_FAVORITE = 2,
    CHANNEL_DIRECTION_SAME     = 3,
} ChannelChangeDirection;

/** \brief TVState is an enumeration of the states used by TV and TVRec.
 */
typedef enum
{
    /** \brief Error State, if we ever try to enter this state errored is set.
     */
    kState_Error = -1,
    /** \brief None State, this is the initial state in both TV and TVRec, it
     *         indicates that we are ready to change to some other state.
     */
    kState_None = 0,
    /** \brief Watching LiveTV is the state for when we are watching a
     *         recording and the user has control over the channel and
     *         the recorder to use.
     */
    kState_WatchingLiveTV,
    /** \brief Watching Pre-recorded is a TV only state for when we are
     *         watching a pre-existing recording.
     */
    kState_WatchingPreRecorded,
    /** \brief Watching Video is the state when we are watching a video and is not
    *             a dvd
    */
    kState_WatchingVideo,
    /** \brief Watching DVD is the state when we are watching a DVD */
    kState_WatchingDVD,
    /** \brief Watching Recording is the state for when we are watching
     *         an in progress recording, but the user does not have control
     *         over the channel and recorder to use.
     */
    kState_WatchingRecording,
    /** \brief Recording Only is a TVRec only state for when we are recording
     *         a program, but there is no one currently watching it.
     */
    kState_RecordingOnly,
    /** \brief This is a placeholder state which we never actualy enter,
     *         but is returned by GetState() when we are in the process
     *         of changing the state.
     */
    kState_ChangingState,
} TVState;

QString StateToString(TVState state, bool brief = false);

/** \brief SleepStatus is an enumeration of the awake/sleep status of a slave.
 */
typedef enum SleepStatus {
	/** \brief A slave is awake when it is connected to the master
      */
    sStatus_Awake         = 0x0,
	/** \brief A slave is considered asleep when it is not awake and not
      *        undefined.
      */
    sStatus_Asleep        = 0x1,
	/** \brief A slave is marked as falling asleep when told to shutdown by
      *        the master.
      */
    sStatus_FallingAsleep = 0x3,
	/** \brief A slave is marked as waking when the master runs the slave's
      *        wakeup command.
      */
    sStatus_Waking        = 0x5,
	/** \brief A slave's sleep status is undefined when it has never connected
      *        to the master backend or is not able to be put to sleep and
      *        awakened.
      */
    sStatus_Undefined     = 0x8
} SleepStatus;

typedef enum PictureAdjustType
{
    kAdjustingPicture_None = 0,
    kAdjustingPicture_Playback,
    kAdjustingPicture_Channel,
    kAdjustingPicture_Recording,
} PictureAdjustType;
QString toTypeString(PictureAdjustType type);
QString toTitleString(PictureAdjustType type);

typedef enum
{
    kCommSkipOff    = 0,
    kCommSkipOn     = 1,
    kCommSkipNotify = 2,
    kCommSkipCount,
    kCommSkipIncr,
} CommSkipMode;
QString toString(CommSkipMode type);
#endif
