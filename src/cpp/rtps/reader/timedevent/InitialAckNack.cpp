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
 * @file InitialAckNack.cpp
 *
 */

#include <fastrtps/rtps/reader/timedevent/InitialAckNack.h>
#include <fastrtps/rtps/resources/ResourceEvent.h>
#include "../StatefulReaderImpl.h"
#include <fastrtps/rtps/reader/WriterProxy.h>
#include "../../participant/RTPSParticipantImpl.h"
#include <fastrtps/rtps/messages/RTPSMessageCreator.h>
#include <fastrtps/log/Log.h>

#include <mutex>

namespace eprosima {
namespace fastrtps{
namespace rtps{

// TODO(Ricardo) Maybe join initial heartbeat, ack and gap

InitialAckNack::~InitialAckNack()
{
    logInfo(RTPS_WRITER,"Destroying InitialAckNack");
    destroy();
}

InitialAckNack::InitialAckNack(WriterProxy& writer_proxy, double interval):
    TimedEvent(writer_proxy.reader_.participant().getEventResource().getIOService(),
            writer_proxy.reader_.participant().getEventResource().getThread(), interval),
    m_cdrmessages(writer_proxy.reader_.participant().getMaxMessageSize(),
            writer_proxy.reader_.participant().guid().guidPrefix), writer_proxy_(writer_proxy)
{
}

void InitialAckNack::event(EventCode code, const char* msg)
{

    // Unused in release mode.
    (void)msg;

    if(code == EVENT_SUCCESS)
    {
        Count_t acknackCount = 0;

        {//BEGIN PROTECTION
            std::lock_guard<std::recursive_mutex> guard_reader(*writer_proxy_.reader_.getMutex());
            writer_proxy_.reader_.m_acknackCount++;
            acknackCount = writer_proxy_.reader_.m_acknackCount;
        }

        // Send initial NACK.
        SequenceNumberSet_t sns;
        sns.base = SequenceNumber_t(0, 0);

        logInfo(RTPS_READER,"Sending ACKNACK: "<< sns);

        RTPSMessageGroup group(writer_proxy_.reader_.participant(), &writer_proxy_.reader_, RTPSMessageGroup::READER, m_cdrmessages);

        LocatorList_t locators(writer_proxy_.m_att.endpoint.unicastLocatorList);
        locators.push_back(writer_proxy_.m_att.endpoint.multicastLocatorList);

        group.add_acknack(writer_proxy_.m_att.guid, sns, acknackCount, false, locators);
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
}
} /* namespace eprosima */
