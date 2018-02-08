// Copyright 2018 Proyectos y Sistemas de Mantenimiento SL (eProsima).
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
 * @file CleanupEvent.cpp
 *
 */

#include "CleanupEvent.h"
#include <fastrtps/rtps/resources/ResourceEvent.h>
#include "../../participant/RTPSParticipantImpl.h"
#include "../StatefulWriterImpl.h"
#include <fastrtps/log/Log.h>

#include <mutex>

namespace eprosima {
namespace fastrtps{
namespace rtps{


CleanupEvent::~CleanupEvent()
{
    logInfo(RTPS_WRITER,"Destroying Cleanup Event");
    destroy();
}

CleanupEvent::CleanupEvent(StatefulWriter::impl& writer, double interval) :
    TimedEvent(writer.participant().getEventResource().getIOService(),
            writer.participant().getEventResource().getThread(), interval),
    writer_(writer)
{
}

void CleanupEvent::cleanup()
{
    writer_.cleanup_();
}

void CleanupEvent::event(EventCode code, const char* msg)
{

    // Unused in release mode.
    (void)msg;

    if(code == EVENT_SUCCESS)
    {
        cleanup();
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

