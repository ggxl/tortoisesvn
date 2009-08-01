/***************************************************************************
 *   Copyright (C) 2009 by Stefan Fuhrmann                                 *
 *   stefanfuhrmann@alice-dsl.de                                           *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "stdafx.h"
#include "WaitableEvent.h"
#include "CriticalSection.h"

// recycler for OS constructs

namespace
{
    class CWaitableEventPool
    {
    private:

        // multi-threading sync.

        CCriticalSection mutex;

        // allocated, currently usused handles

        std::vector<HANDLE> handles;

        // construction / destruction:
        // free all handles upon destruction at latest

        CWaitableEventPool();
        ~CWaitableEventPool();

    public:

        // Meyer's singleton

        static CWaitableEventPool* GetInstance();

        // recycling interface: 
        // alloc at request

        HANDLE Alloc();
        void AutoAlloc (HANDLE& handle);
        void Release (HANDLE event);
        void Clear();
    };

    // construction / destruction:

    CWaitableEventPool::CWaitableEventPool()
    {
    }

    CWaitableEventPool::~CWaitableEventPool()
    {
        Clear();
    }

    // Meyer's singleton

    CWaitableEventPool* CWaitableEventPool::GetInstance()
    {
        static CWaitableEventPool instance;
        return &instance;
    }

    // recycling interface: 
    // alloc at request

    HANDLE CWaitableEventPool::Alloc()
    {
        {
            CCriticalSectionLock lock (mutex);
            if (!handles.empty())
            {
                HANDLE result = *handles.rbegin();
                handles.pop_back();
                return result;
            }
        }

        return CreateEvent (NULL, TRUE, FALSE, NULL);
    }

    void CWaitableEventPool::AutoAlloc (HANDLE& handle)
    {
        CCriticalSectionLock lock (mutex);
        if (handle != INVALID_HANDLE_VALUE)
            return;

        if (!handles.empty())
        {
            handle = *handles.rbegin();
            handles.pop_back();
        }
        else
        {
            handle = CreateEvent (NULL, TRUE, FALSE, NULL);
        }
    }

    void CWaitableEventPool::Release (HANDLE event)
    {
        ResetEvent (event);

        CCriticalSectionLock lock (mutex);
        handles.push_back (event);
    }

    void CWaitableEventPool::Clear()
    {
        CCriticalSectionLock lock (mutex);

        while (!handles.empty())
            CloseHandle (Alloc());
    }
}

// construction / destruction: manage event handle

COneShotEvent::COneShotEvent()
    : event (INVALID_HANDLE_VALUE)
    , state (FALSE)
{
}

COneShotEvent::~COneShotEvent()
{
    if (event != INVALID_HANDLE_VALUE)
        CWaitableEventPool::GetInstance()->Release (event);
}

// eventing interface

void COneShotEvent::Set()
{
    if (InterlockedExchange (&state, TRUE) == FALSE)
        if (event != INVALID_HANDLE_VALUE)
            SetEvent (event);
}

bool COneShotEvent::Test() const
{
    return state == TRUE;
}

void COneShotEvent::WaitFor()
{
    if (state == FALSE)
    {
        CWaitableEventPool::GetInstance()->AutoAlloc (event);
        if (state == FALSE)
            WaitForSingleObject (event, INFINITE);
    }
}

// construction / destruction: manage event handle

CWaitableEvent::CWaitableEvent()
    : event (CWaitableEventPool::GetInstance()->Alloc())
{
}

CWaitableEvent::~CWaitableEvent()
{
    CWaitableEventPool::GetInstance()->Release (event);
}

// eventing interface

void CWaitableEvent::Set()
{
    SetEvent (event);
}

void CWaitableEvent::Reset()
{
    ResetEvent (event);
}

bool CWaitableEvent::Test() const
{
    return WaitForSingleObject (event, 0) == WAIT_OBJECT_0;
}

void CWaitableEvent::WaitFor()
{
    WaitForSingleObject (event, INFINITE);
}
