// Copyright 2016 Proyectos y Sistemas de Mantenimiento SL (eProsima).
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/**
 * @file NackResponseDelay.cpp
 *
 */

#include <fastrtps/rtps/writer/timedevent/NackResponseDelay.h>
#include <fastrtps/rtps/writer/timedevent/NackSupressionDuration.h>
#include <fastrtps/rtps/writer/timedevent/PeriodicHeartbeat.h>
#include <fastrtps/rtps/resources/ResourceEvent.h>
#include <fastrtps/rtps/resources/AsyncWriterThread.h>

#include <fastrtps/rtps/writer/StatefulWriter.h>
#include <fastrtps/rtps/writer/ReaderProxy.h>
#include "../../participant/RTPSParticipantImpl.h"

#include <fastrtps/log/Log.h>

#include <fastrtps/rtps/messages/RTPSMessageCreator.h>

#include <mutex>

using namespace eprosima::fastrtps::rtps;

NackResponseDelay::~NackResponseDelay()
{
    destroy();
}

NackResponseDelay::NackResponseDelay(ReaderProxy& remote_reader,double millisec):
    TimedEvent(remote_reader.mp_SFW->getRTPSParticipant()->getEventResource().getIOService(),
            remote_reader.mp_SFW->getRTPSParticipant()->getEventResource().getThread(), millisec),
    remote_reader_(remote_reader)
{
}

void NackResponseDelay::event(EventCode code, const char*)
{
    if(code == EVENT_SUCCESS)
    {
        logInfo(RTPS_WRITER,"Responding to Acknack msg";);
        remote_reader_.convert_status_on_all_changes(REQUESTED, UNSENT);
    }
}
