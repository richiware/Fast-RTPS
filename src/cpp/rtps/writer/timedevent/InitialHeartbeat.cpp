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
 * @file InitialHeartbeat.cpp
 *
 */

#include <fastrtps/rtps/writer/timedevent/InitialHeartbeat.h>
#include <fastrtps/rtps/resources/ResourceEvent.h>
#include <fastrtps/rtps/writer/ReaderProxy.h>
#include "../../participant/RTPSParticipantImpl.h"
#include "../StatefulWriterImpl.h"
#include <fastrtps/log/Log.h>

#include <mutex>

namespace eprosima {
namespace fastrtps{
namespace rtps{


InitialHeartbeat::~InitialHeartbeat()
{
    logInfo(RTPS_WRITER,"Destroying InitialHB");
    destroy();
}

InitialHeartbeat::InitialHeartbeat(const ReaderProxy& remote_reader, double interval) :
    TimedEvent(remote_reader.writer_.participant().getEventResource().getIOService(),
            remote_reader.writer_.participant().getEventResource().getThread(), interval),
    remote_reader_(remote_reader)
{
}

void InitialHeartbeat::event(EventCode code, const char* msg)
{

    // Unused in release mode.
    (void)msg;

    if(code == EVENT_SUCCESS)
    {
        remote_reader_.writer_.send_heartbeat_to(remote_reader_, false);
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
