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
 * @file NackSupressionDuration.cpp
 *
 */

#include "NackSupressionDuration.h"
#include <fastrtps/rtps/resources/ResourceEvent.h>
#include "../StatefulWriterImpl.h"
#include <fastrtps/rtps/writer/ReaderProxy.h>
#include <fastrtps/rtps/writer/timedevent/PeriodicHeartbeat.h>
#include "../../participant/RTPSParticipantImpl.h"
#include <fastrtps/log/Log.h>

#include <mutex>

namespace eprosima {
namespace fastrtps{
namespace rtps {


NackSupressionDuration::~NackSupressionDuration()
{
    destroy();
}

NackSupressionDuration::NackSupressionDuration(ReaderProxy& remote_reader, double millisec) :
    TimedEvent(remote_reader.writer_.participant().getEventResource().getIOService(),
            remote_reader.writer_.participant().getEventResource().getThread(), millisec),
    remote_reader_(remote_reader)
{

}

void NackSupressionDuration::restart_timer()
{
    remote_reader_.convert_status_on_all_changes(UNDERWAY,UNACKNOWLEDGED);
    remote_reader_.writer_.periodic_heartbeat_->restart_timer();
}

void NackSupressionDuration::event(EventCode code, const char* msg)
{

    // Unused in release mode.
    (void)msg;

    if(code == EVENT_SUCCESS)
    {
        logInfo(RTPS_WRITER,"Changing underway to unacked for Reader: " << remote_reader_.m_att.guid);

        if(remote_reader_.m_att.endpoint.reliabilityKind == RELIABLE)
        {
            restart_timer();
        }
    }
    else if(code == EVENT_ABORT)
    {
        logInfo(RTPS_WRITER,"Aborted");
    }
    else
    {
        logInfo(RTPS_WRITER,"Event message: " <<msg);
    }
}
}
} /* namespace dds */
} /* namespace eprosima */
