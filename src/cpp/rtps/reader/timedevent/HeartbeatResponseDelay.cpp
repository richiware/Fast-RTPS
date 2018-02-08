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
 * @file HeartbeatResponseDelay.cpp
 *
 */

#include <fastrtps/rtps/reader/timedevent/HeartbeatResponseDelay.h>
#include <fastrtps/rtps/resources/ResourceEvent.h>
#include <fastrtps/rtps/reader/WriterProxy.h>
#include "../StatefulReaderImpl.h"
#include "../../participant/RTPSParticipantImpl.h"
#include <fastrtps/rtps/messages/RTPSMessageCreator.h>
#include <fastrtps/rtps/messages/CDRMessage.h>
#include <fastrtps/log/Log.h>

#include <mutex>

namespace eprosima {
namespace fastrtps{
namespace rtps {


HeartbeatResponseDelay::~HeartbeatResponseDelay()
{
    destroy();
}

HeartbeatResponseDelay::HeartbeatResponseDelay(WriterProxy& writer_proxy, double interval):
    TimedEvent(writer_proxy.reader_.participant().getEventResource().getIOService(),
            writer_proxy.reader_.participant().getEventResource().getThread(), interval),
    writer_proxy_(writer_proxy), m_cdrmessages(writer_proxy.reader_.participant().getMaxMessageSize(),
            writer_proxy.reader_.participant().guid().guidPrefix)
{

}

void HeartbeatResponseDelay::event(EventCode code, const char* msg)
{

    // Unused in release mode.
    (void)msg;

    if(code == EVENT_SUCCESS)
    {
        logInfo(RTPS_READER,"");

        // Protect reader
        std::lock_guard<std::recursive_mutex> guard(*writer_proxy_.reader_.getMutex());

        const std::vector<ChangeFromWriter_t> missing_changes = writer_proxy_.missing_changes();
        // Stores missing changes but there is some fragments received.
        std::vector<CacheChange_t*> uncompleted_changes;

        RTPSMessageGroup group(writer_proxy_.reader_.participant(), &writer_proxy_.reader_, RTPSMessageGroup::READER, m_cdrmessages);
        LocatorList_t locators(writer_proxy_.m_att.endpoint.unicastLocatorList);
        locators.push_back(writer_proxy_.m_att.endpoint.multicastLocatorList);

        if(!missing_changes.empty() || !writer_proxy_.m_heartbeatFinalFlag)
        {
            SequenceNumberSet_t sns;
            sns.base = writer_proxy_.available_changes_max();
            sns.base++;

            for(auto ch : missing_changes)
            {
                // Check if the CacheChange_t is uncompleted.
                CacheChange_t* uncomplete_change = writer_proxy_.reader_.findCacheInFragmentedCachePitStop(
                        ch.getSequenceNumber(), writer_proxy_.m_att.guid);

                if(uncomplete_change == nullptr)
                {
                    if(!sns.add(ch.getSequenceNumber()))
                    {
                        logInfo(RTPS_READER,"Sequence number " << ch.getSequenceNumber()
                                << " exceeded bitmap limit of AckNack. SeqNumSet Base: " << sns.base);
                    }
                }
                else
                {
                    uncompleted_changes.push_back(uncomplete_change);
                }
            }

            // TODO Protect
            writer_proxy_.reader_.m_acknackCount++;
            logInfo(RTPS_READER,"Sending ACKNACK: "<< sns;);

            bool final = false;
            if(sns.isSetEmpty())
                final = true;

            group.add_acknack(writer_proxy_.m_att.guid, sns, writer_proxy_.reader_.m_acknackCount, final, locators);
        }

        // Now generage NACK_FRAGS
        if(!uncompleted_changes.empty())
        {
            for(auto cit : uncompleted_changes)
            {
                FragmentNumberSet_t frag_sns;

                //  Search first fragment not present.
                uint32_t frag_num = 0;
                auto fit = cit->getDataFragments()->begin();
                for(; fit != cit->getDataFragments()->end(); ++fit)
                {
                    ++frag_num;
                    if(*fit == ChangeFragmentStatus_t::NOT_PRESENT)
                        break;
                }

                // Never should happend.
                assert(frag_num != 0);
                assert(fit != cit->getDataFragments()->end());

                // Store FragmentNumberSet_t base.
                frag_sns.base = frag_num;

                // Fill the FragmentNumberSet_t bitmap.
                for(; fit != cit->getDataFragments()->end(); ++fit)
                {
                    if(*fit == ChangeFragmentStatus_t::NOT_PRESENT)
                        frag_sns.add(frag_num);

                    ++frag_num;
                }

                ++writer_proxy_.reader_.m_nackfragCount;
                logInfo(RTPS_READER,"Sending NACKFRAG for sample" << cit->sequence_number << ": "<< frag_sns;);

                group.add_nackfrag(writer_proxy_.m_att.guid, cit->sequence_number, frag_sns, writer_proxy_.reader_.m_nackfragCount, locators);
            }
        }
    }
    else if(code == EVENT_ABORT)
    {
        logInfo(RTPS_READER,"HeartbeatResponseDelay aborted");
    }
    else
    {
        logInfo(RTPS_READER,"HeartbeatResponseDelay event message: " <<msg);
    }
}

}
} /* namespace rtps */
} /* namespace eprosima */
