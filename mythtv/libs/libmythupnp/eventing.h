//////////////////////////////////////////////////////////////////////////////
// Program Name: Eventing.h
// Created     : Dec. 22, 2006
//
// Purpose     : uPnp Eventing Base Class Definition
//                                                                            
// Copyright (c) 2006 David Blain <mythtv@theblains.net>
//                                          
// This library is free software; you can redistribute it and/or 
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or at your option any later version of the LGPL.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#ifndef EVENTING_H_
#define EVENTING_H_

#include <sys/time.h>

#include <QUrl>
#include <QUuid>
#include <QMap>

#include "upnpimpl.h"
#include "upnputil.h"
#include "httpserver.h"

class QTextStream;

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////
        
class SubscriberInfo
{
    public:
        SubscriberInfo()
            : nKey( 0 ), nDuration( 0 )
        {
            bzero( &ttExpires, sizeof( ttExpires ) );
            bzero( &ttLastNotified, sizeof( ttLastNotified ) );
            sUUID          = QUuid::createUuid().toString();
            sUUID          = sUUID.mid( 1, sUUID.length() - 2);
        }

        SubscriberInfo( const QString &url, unsigned long duration )
            : nKey( 0 ), nDuration( duration )
        {
            bzero( &ttExpires, sizeof( ttExpires ) );
            bzero( &ttLastNotified, sizeof( ttLastNotified ) );
            sUUID          = QUuid::createUuid().toString();
            sUUID          = sUUID.mid( 1, sUUID.length() - 2);
            qURL           = url;

            SetExpireTime( nDuration );
        }

        unsigned long IncrementKey()
        {
            // When key wraps around to zero again... must make it a 1. (upnp spec)
            if ((++nKey) == 0)
                nKey = 1;

            return nKey;
        }

        TaskTime            ttExpires;
        TaskTime            ttLastNotified;

        QString             sUUID;
        QUrl                qURL;
        unsigned short      nKey;
        unsigned long       nDuration;       // Seconds

    protected:

        void SetExpireTime( unsigned long nSecs )
        {
            TaskTime tt;
            gettimeofday( &tt, NULL );

            AddMicroSecToTaskTime( tt, (nSecs * 1000000) );

            ttExpires = tt;
        }


};

//////////////////////////////////////////////////////////////////////////////

typedef QMap<QString,SubscriberInfo*> Subscribers;

//////////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////////

class StateVariableBase
{
    public:

        bool        m_bNotify;
        QString     m_sName;
        TaskTime    m_ttLastChanged;

    public:

        StateVariableBase( const QString &sName, bool bNotify = false ) 
        {
            m_bNotify = bNotify;
            m_sName   = sName;
            gettimeofday( &m_ttLastChanged, NULL );
        }

        virtual QString ToString() = 0;
};
  
//////////////////////////////////////////////////////////////////////////////

template< class T >
class StateVariable : public StateVariableBase
{
    private:
        
        T     m_value;

    public:

        // ------------------------------------------------------------------

        StateVariable( const QString &sName, bool bNotify = false ) : StateVariableBase( sName, bNotify )
        {
        }

        // ------------------------------------------------------------------

        StateVariable( const QString &sName, T value, bool bNotify = false ) : StateVariableBase( sName, bNotify )
        {
            m_value = value;
        }

        // ------------------------------------------------------------------

        virtual QString ToString()
        {
            return QString( "%1" ).arg( m_value );            
        }

        // ------------------------------------------------------------------

        T GetValue()             
        { 
            return m_value; 
        }

        // ------------------------------------------------------------------

        void SetValue( T value )
        {
            if ( m_value != value )
            {
                m_value = value;
                gettimeofday( &m_ttLastChanged, NULL );
            }
        }
};

//////////////////////////////////////////////////////////////////////////////

class StateVariables
{
    protected:

        virtual void Notify() = 0;
        typedef QMap<QString, StateVariableBase*> SVMap;
        SVMap m_map;
    public:

        // ------------------------------------------------------------------

        StateVariables() { }     
        ~StateVariables()
        {
            SVMap::iterator it = m_map.begin();
            for (; it != m_map.end(); ++it)
                delete *it;
            m_map.clear();
        }     

        // ------------------------------------------------------------------

        void AddVariable( StateVariableBase *pBase )
        {
            if (pBase != NULL)
                m_map.insert(pBase->m_sName, pBase);
        }

        // ------------------------------------------------------------------
        template < class T >
        bool SetValue( const QString &sName, T value )
        {
            SVMap::iterator it = m_map.find(sName);
            if (it == m_map.end())
                return false;

            StateVariable< T > *pVariable =
                dynamic_cast< StateVariable< T > *>( *it );

            if (pVariable == NULL)
                return false;           // It's not the expected type.

            if ( pVariable->GetValue() != value)
            {
                pVariable->SetValue( value );

                if (pVariable->m_bNotify)
                    Notify();
            }

            return true;
        }

        // ------------------------------------------------------------------

        template < class T >
        T GetValue( const QString &sName )
        {
            SVMap::iterator it = m_map.find(sName);
            if (it == m_map.end())
                return T(0);

            StateVariable< T > *pVariable =
                dynamic_cast< StateVariable< T > *>( *it );

            if (pVariable != NULL)
                return pVariable->GetValue();

            return T(0);
        }

        uint BuildNotifyBody(QTextStream &ts, TaskTime ttLastNotified) const;
};

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
// Eventing Class Definition
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

class Eventing : public HttpServerExtension,
                 public StateVariables,
                 public IPostProcess,
                 public UPnpServiceImpl
{

    protected:

        QMutex              m_mutex;

        QString             m_sEventMethodName;
        Subscribers         m_Subscribers;

        int                 m_nSubscriptionDuration;

        short               m_nHoldCount;

        SubscriberInfo     *m_pInitializeSubscriber;

    protected:

        virtual void Notify           ( );
        void         NotifySubscriber ( SubscriberInfo *pInfo );
        void         HandleSubscribe  ( HTTPRequest *pRequest ); 
        void         HandleUnsubscribe( HTTPRequest *pRequest ); 

        // Implement UPnpServiceImpl methods that we can

        virtual QString GetServiceEventURL  () { return m_sEventMethodName; }

    public:
                 Eventing      ( const QString &sExtensionName, const QString &sEventMethodName ); 
        virtual ~Eventing      ( );

        virtual bool ProcessRequest( HttpWorkerThread *pThread, HTTPRequest *pRequest );

        short    HoldEvents    ( );
        short    ReleaseEvents ( );

        void     ExecutePostProcess( );


};

#endif
