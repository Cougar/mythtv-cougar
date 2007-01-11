/* Definition of the ZMServer class.
 * ============================================================
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published bythe Free Software Foundation;
 * either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * ============================================================ */

#ifndef ZMSERVER_H
#define ZMSERVER_H


#include <unistd.h>
#include <string>
#include <vector>
#include <map>
#include <mysql/mysql.h>

#include "config.h"

using namespace std;

extern void loadZMConfig(const string &configfile);
extern void connectToDatabase(void);

// there are shared by all ZMServer's
extern MYSQL   g_dbConn;
extern string  g_password;
extern string  g_server;
extern string  g_database;
extern string  g_webPath;
extern string  g_user;
extern string  g_webUser;
extern string  g_binPath;


typedef enum 
{ 
    IDLE,
    PREALARM,
    ALARM,
    ALERT,
    TAPE
} State;

typedef struct
{
    int size;
    bool valid;
    bool active;
    bool signal;
    State state;
    int last_write_index;
    int last_read_index;
    time_t last_image_time;
    int last_event;
    int action;
    int brightness;
    int hue;
    int colour;
    int contrast;
    int alarm_x;
    int alarm_y;
    char control_state[256];
} SharedData; 

typedef enum { TRIGGER_CANCEL, TRIGGER_ON, TRIGGER_OFF } TriggerState;

typedef struct
{
    int size;
    TriggerState trigger_state;
    int trigger_score;
    char trigger_cause[32];
    char trigger_text[256];
#ifdef ZMVERSION_1_22_2
    char trigger_showtext[32];
#else
    char trigger_showtext[256];
#endif
} TriggerData;

typedef struct
{
    string name;
    int image_buffer_count;
    int width;
    int height;
    int mon_id;
    SharedData *shared_data;
    unsigned char *shared_images;
    int last_read;
    string status;
    int frame_size;
    int palette;
} MONITOR;

class ZMServer
{
  public:
    ZMServer(int sock, bool debug);
    ~ZMServer();

    void processRequest(char* buf, int nbytes);

  private:
    bool send(const string s) const;
    bool send(const string s, const unsigned char *buffer, int dataLen) const;
    void sendError(string error);
    void getMonitorList(void);
    void initMonitor(MONITOR *monitor);
    int  getFrame(unsigned char *buffer, int bufferSize, MONITOR *monitor);
    long long getDiskSpace(const string &filename, long long &total, long long &used);
    void tokenize(const string &command, vector<string> &tokens);
    void handleHello(void);
    string runCommand(string command);
    void getMonitorStatus(string id, string type, string device, string channel,
                          string function, string &zmcStatus, string &zmaStatus);
    void handleGetServerStatus(void);
    void handleGetMonitorStatus(void);
    void handleGetMonitorList(void);
    void handleGetCameraList(void);
    void handleGetEventList(vector<string> tokens);
    void handleGetEventFrame(vector<string> tokens);
    void handleGetLiveFrame(vector<string> tokens);
    void handleGetFrameList(vector<string> tokens);
    void handleDeleteEvent(vector<string> tokens);

    bool                 m_debug;
    int                  m_sock;
    map<int, MONITOR *>  m_monitors;
};


#endif
