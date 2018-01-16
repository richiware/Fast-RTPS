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
 * @file PeriodicHeartbeat.cpp
 *
 */

#include <fastrtps/rtps/writer/timedevent/PeriodicHeartbeat.h>
#include <fastrtps/rtps/resources/ResourceEvent.h>

#include <fastrtps/rtps/writer/StatefulWriter.h>
#include <fastrtps/rtps/writer/ReaderProxy.h>

#include "../../participant/RTPSParticipantImpl.h"

#include <fastrtps/rtps/messages/RTPSMessageCreator.h>

#include <fastrtps/log/Log.h>

#include <mutex>

namespace eprosima {
namespace fastrtps{
namespace rtps{


PeriodicHeartbeat::~PeriodicHeartbeat()
{
    logInfo(RTPS_WRITER,"Destroying PeriodicHB");
    destroy();
}

PeriodicHeartbeat::PeriodicHeartbeat(StatefulWriter& writer, double interval):
    TimedEvent(writer.getRTPSParticipant()->getEventResource().getIOService(),
            writer.getRTPSParticipant()->getEventResource().getThread(), interval), writer_(writer)
{

}

void PeriodicHeartbeat::event(EventCode code, const char* msg)
{

    // Unused in release mode.
    (void)msg;

    if(code == EVENT_SUCCESS)
    {
        if(!writer_.wait_for_all_acked(Duration_t()))
        {
            writer_.send_heartbeat(false);

            //Reset TIMER
            this->restart_timer();
        }
    }
    else if(code == EVENT_ABORT)
    {
        logInfo(RTPS_WRITER,"Aborted");
    }
    else
    {
        logInfo(RTPS_WRITER,"Event message: " << msg);
    }
}

}
}
} /* namespace eprosima */
