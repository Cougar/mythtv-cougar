#ifndef JOBTHREAD_H_
#define JOBTHREAD_H_
/*
	jobthread.h

	(c) 2003 Thor Sigvaldason and Isaac Richards
	Part of the mythTV project
	
	Headers for the mtd threads that actually do things

*/

#include <qthread.h>
#include <qstringlist.h>
#include <qfile.h>
#include <qsqldatabase.h>
#include <qprocess.h>
#include <dvdread/dvd_reader.h>
#include <dvdread/ifo_types.h>
#include <dvdread/ifo_read.h>
#include <dvdread/nav_read.h>

#include "fileobs.h"

class MTD;

class JobThread : public QThread
{
    
    //
    //  A base class for all mtd threads
    //

  public:
  
    JobThread(MTD *owner, const QString &start_string, int nice_priority);
    virtual void run();
    bool keepGoing();
    
    QString getJobName(){return job_name;}
    QString getSubName(){return subjob_name;}
    double  getProgress(){return overall_progress;}
    double  getSubProgress(){return subjob_progress;}
    
    void    problem(const QString &a_problem){problem_string = a_problem;}
    QString getProblem(){return problem_string;}
    QString getJobString(){return job_string;}
    void    updateSubjobString( int seconds_elapsed, 
                                const QString &pre_string);
    void    cancelMe(bool yes_or_no){cancel_me = yes_or_no;}
    void    setSubProgress(double a_value, uint priority);
    void    setSubName(const QString &new_name, uint priority);
    virtual QString getFinalFileName(){return "";}
    
  protected:
  
    QString job_name;
    QString subjob_name;
    double  overall_progress;
    double  subjob_progress;
    double  sub_to_overall_multiple;
    MTD     *parent;
    QString problem_string;
    QString job_string;
    int     nice_level;
    bool    cancel_me;
    
    QMutex  sub_progress_mutex;
    QMutex  sub_name_mutex;
};


class DVDThread : public JobThread
{

    //
    //  Base class for all *DVD* related
    //  job threads (perfect copy, transcode)
    //

  public:
  
    DVDThread(MTD *owner,
              QMutex *drive_mutex,
              const QString &dvd_device,
              int track,
              const QString &dest_file, 
              const QString &name,
              const QString &start_string,
              int nice_priority);

    ~DVDThread();
                     
    virtual void run();
    QString getFinalFileName(){return destination_file_string;}

  protected:

    bool         ripTitle(int title_number,
                          const QString &to_location,
                          const QString &extension,
                          bool multiple_files);
  
    RipFile      *ripfile;
    QMutex       *dvd_device_access;
    QString      dvd_device_location;
    QString      destination_file_string;
    int          dvd_title;
    dvd_reader_t *the_dvd;
    dvd_file_t   *title;
    unsigned char video_data[ 1024 * DVD_VIDEO_LB_LEN ];
    QString      rip_name;
};

class DVDPerfectThread : public DVDThread
{
    //
    //  Fairly simple class that just knows
    //  how to copy
    //
    
  public:
  
    DVDPerfectThread(MTD *owner,
                     QMutex *drive_mutex, 
                     const QString &dvd_device, 
                     int track, 
                     const QString &dest_file, 
                     const QString &name,
                     const QString &start_string,
                     int nice_priority);

    ~DVDPerfectThread();
                     
    virtual void run();
    

};

class DVDTranscodeThread : public DVDThread
{
    
    //
    //  An object that can rip a VOB off a DVD
    //  and then transcode it
    //
    
  public:
  
    DVDTranscodeThread(MTD *owner,
                       QMutex *drive_mutex,
                       const QString &dvd_device,
                       int track,
                       const QString &dest_file,
                       const QString &name,
                       const QString &start_string,
                       int nice_priority,
                       int quality_level,
                       bool do_ac3,
                       QSqlDatabase *ldb,
                       int which_audio,
                       int numb_seconds,
                       int subtitle_track_numb);
                       
                      
    ~DVDTranscodeThread();
    
    virtual void run();
    
    bool    makeWorkingDirectory();
    bool    buildTranscodeCommandLine();
    bool    runTranscode(int run);
    void    cleanUp();
    void    wipeClean();
    
  private:
  
    int          quality;
    QDir         *working_directory;
    QStringList  tc_arguments;
    QProcess     *tc_process;
    bool         two_pass;
    QSqlDatabase *db;
    int          audio_track;
    int          length_in_seconds;
    bool         ac3_flag;
    int          subtitle_track;
};


#endif  // jobthread_h_

