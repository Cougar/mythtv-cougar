//////////////////////////////////////////////////////////////////////////////
// Program Name: taskqueue.cpp
//                                                                            
// Purpose - Used to process delayed tasks
//                                                                            
// Created By  : David Blain                    Created On : Oct. 24, 2005
// Modified By :                                Modified On:                  
//                                                                            
//////////////////////////////////////////////////////////////////////////////

#include "taskqueue.h"
#include <sys/time.h>
#include <qdatetime.h>

#include <iostream>

using std::cerr;

/////////////////////////////////////////////////////////////////////////////
// Define Global instance 
/////////////////////////////////////////////////////////////////////////////

TaskQueue *g_pTaskQueue;

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// 
// Task Implementation
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

long Task::m_nTaskCount = 0;

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

Task::Task()
{
    m_nTaskId = m_nTaskCount++;
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

Task::~Task()
{
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//
// Task Queue Implementation
//
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

TaskQueue::TaskQueue() : m_bTermRequested( false )
{

}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

TaskQueue::~TaskQueue()
{
    m_bTermRequested = true; 

    wait();

    Clear();
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void TaskQueue::run( )
{
    Task *pTask;

    while ( !m_bTermRequested )
    {
        // ------------------------------------------------------------------
        // Process Any Tasks that may need to be executed.
        // ------------------------------------------------------------------

        TaskTime ttNow;
        gettimeofday( &ttNow, NULL );

        if ((pTask = GetNextExpiredTask( ttNow )) != NULL)
        {
            try
            {
                pTask->Execute( this );
                pTask->Release();
            }
            catch( ... )
            {
                cerr << "TaskQueue::run - Call to Execute threw an exception.";
            }

        }
        // Make sure to throttle our processing.

        msleep( 100 );
    }

}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void TaskQueue::Clear( )
{
    m_mutex.lock(); 

    for ( TaskMap::iterator it  = m_mapTasks.begin();
                            it != m_mapTasks.end();
                          ++it )
    {
        if ((*it).second != NULL)
            (*it).second->Release();
    }

    m_mapTasks.clear();

    m_mutex.unlock(); 
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void TaskQueue::AddTask( long msec, Task *pTask )
{
    TaskTime tt;
    gettimeofday( &tt, NULL );

    AddMicroSecToTaskTime( tt, (msec * 1000) );

    AddTask( tt, pTask );
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void TaskQueue::AddTask( TaskTime ttKey, Task *pTask )
{

    if (pTask != NULL)
    {
        m_mutex.lock(); 
        pTask->AddRef();
        m_mapTasks.insert( TaskMap::value_type( ttKey, pTask ));
        m_mutex.unlock();
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

void TaskQueue::AddTask( Task *pTask )
{

    if (pTask != NULL)
    {
        TaskTime tt;
        gettimeofday( &tt, NULL );

        AddTask( tt, pTask );
    }
}

/////////////////////////////////////////////////////////////////////////////
//
/////////////////////////////////////////////////////////////////////////////

Task *TaskQueue::GetNextExpiredTask( TaskTime tt, long nWithinMilliSecs /*=50*/ )
{
    Task *pTask = NULL;

    AddMicroSecToTaskTime( tt, nWithinMilliSecs * 1000 );

    m_mutex.lock(); 

    TaskMap::iterator it = m_mapTasks.begin();

    if (it != m_mapTasks.end())
    {
        TaskTime ttTask = (*it).first;

        if (ttTask < tt)
        {
            // Do not release here... caller must call release.

            pTask = (*it).second;
        
            m_mapTasks.erase( it );    
        }
    }
    m_mutex.unlock(); 

    return pTask;
}
