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
 * @file WLivelinessPeriodicAssertion.cpp
 *
 */

#include <fastrtps/rtps/builtin/liveliness/timedevent/WLivelinessPeriodicAssertion.h>
#include <fastrtps/rtps/resources/ResourceEvent.h>
#include <fastrtps/rtps/builtin/liveliness/WLP.h>
#include "../../../participant/RTPSParticipantImpl.h"

#include <fastrtps/rtps/writer/StatefulWriter.h>
#include "../../../history/WriterHistoryImpl.h"
#include <fastrtps/rtps/builtin/data/ParticipantProxyData.h>

#include <fastrtps/log/Log.h>
#include <fastrtps/utils/eClock.h>

#include <fastrtps/rtps/builtin/discovery/participant/PDPSimple.h>
#include <fastrtps/rtps/builtin/BuiltinProtocols.h>

#include <mutex>


namespace eprosima {
namespace fastrtps{
namespace rtps {


WLivelinessPeriodicAssertion::WLivelinessPeriodicAssertion(WLP& wlp, LivelinessQosPolicyKind kind) :
    TimedEvent(wlp.participant().getEventResource().getIOService(),
            wlp.participant().getEventResource().getThread(), 0),
    m_livelinessKind(kind), wlp_(wlp)
    {
        m_guidP = wlp.participant().guid().guidPrefix;
        for(uint8_t i =0;i<12;++i)
        {
            m_iHandle.value[i] = m_guidP.value[i];
        }
        m_iHandle.value[15] = m_livelinessKind+0x01;
    }

WLivelinessPeriodicAssertion::~WLivelinessPeriodicAssertion()
{
    destroy();
}

void WLivelinessPeriodicAssertion::event(EventCode code, const char* msg)
{

    // Unused in release mode.
    (void)msg;

    if(code == EVENT_SUCCESS)
    {
        logInfo(RTPS_LIVELINESS,"Period: "<< this->getIntervalMilliSec());
        //TODO Lock writer!!
        if(wlp_.mp_builtinWriter->getMatchedReadersSize()>0)
        {
            if(m_livelinessKind == AUTOMATIC_LIVELINESS_QOS)
                AutomaticLivelinessAssertion();
            else if(m_livelinessKind == MANUAL_BY_PARTICIPANT_LIVELINESS_QOS)
                ManualByRTPSParticipantLivelinessAssertion();
        }
        wlp_.pdpsimple_->assertLocalWritersLiveliness(m_livelinessKind);
        this->restart_timer();
    }
    else if(code == EVENT_ABORT)
    {
        logInfo(RTPS_LIVELINESS,"Liveliness Periodic Assertion aborted");
    }
    else
    {
        logInfo(RTPS_LIVELINESS,"Message: " <<msg);
    }
}

bool WLivelinessPeriodicAssertion::AutomaticLivelinessAssertion()
{
    std::lock_guard<std::recursive_mutex> guard(*wlp_.pdpsimple_->getMutex());
    if(wlp_.m_livAutomaticWriters.size()>0)
    {
        // TODO Reuse change.
        CacheChange_ptr change = wlp_.mp_builtinWriter->new_change([]() ->
                uint32_t {return BUILTIN_PARTICIPANT_DATA_MAX_SIZE;}, ALIVE,m_iHandle);

        if(change)
        {
            //change->instanceHandle = m_iHandle;
#if EPROSIMA_BIG_ENDIAN
            change->serialized_payload.encapsulation = (uint16_t)PL_CDR_BE;
#else
            change->serialized_payload.encapsulation = (uint16_t)PL_CDR_LE;
#endif
            memcpy(change->serialized_payload.data,m_guidP.value,12);
            for(uint8_t i =12;i<24;++i)
                change->serialized_payload.data[i] = 0;
            change->serialized_payload.data[15] = m_livelinessKind+1;
            change->serialized_payload.length = 12+4+4+4;

            //TODO View if can use history_.reuse
            {
                wlp_.mp_builtinWriterHistory->lock_for_transaction();
                for(auto ch = wlp_.mp_builtinWriterHistory->begin_nts(); ch != wlp_.mp_builtinWriterHistory->end_nts(); ++ch)
                {
                    if(ch->second->instance_handle == change->instance_handle)
                    {
                        wlp_.mp_builtinWriterHistory->remove_change(ch->second->sequence_number);
                        break;
                    }
                }
            }

            wlp_.mp_builtinWriterHistory->add_change(change);
        }
    }
    return true;
}

bool WLivelinessPeriodicAssertion::ManualByRTPSParticipantLivelinessAssertion()
{
    std::lock_guard<std::recursive_mutex> guard(*wlp_.pdpsimple_->getMutex());
    bool livelinessAsserted = false;

    for(auto wit = wlp_.m_livManRTPSParticipantWriters.begin();
            wit != wlp_.m_livManRTPSParticipantWriters.end(); ++wit)
    {
        if((*wit)->getLivelinessAsserted())
        {
            livelinessAsserted = true;
        }
        (*wit)->setLivelinessAsserted(false);
    }

    if(livelinessAsserted)
    {
        CacheChange_ptr change = wlp_.mp_builtinWriter->new_change([]() ->
                uint32_t {return BUILTIN_PARTICIPANT_DATA_MAX_SIZE;}, ALIVE);

        if(change)
        {
            change->instance_handle = m_iHandle;
#if EPROSIMA_BIG_ENDIAN
            change->serialized_payload.encapsulation = (uint16_t)PL_CDR_BE;
#else
            change->serialized_payload.encapsulation = (uint16_t)PL_CDR_LE;
#endif
            memcpy(change->serialized_payload.data,m_guidP.value,12);

            for(uint8_t i =12;i<24;++i)
                change->serialized_payload.data[i] = 0;
            change->serialized_payload.data[15] = m_livelinessKind+1;
            change->serialized_payload.length = 12+4+4+4;

            //TODO View if can use history_.reuse
            {
                wlp_.mp_builtinWriterHistory->lock_for_transaction();
                for(auto ch = wlp_.mp_builtinWriterHistory->begin_nts(); ch != wlp_.mp_builtinWriterHistory->end_nts(); ++ch)
                {
                    if(ch->second->instance_handle == change->instance_handle)
                    {
                        wlp_.mp_builtinWriterHistory->remove_change(ch->second->sequence_number);
                        break;
                    }
                }
            }

            wlp_.mp_builtinWriterHistory->add_change(change);
        }
    }

    return false;
}

}
} /* namespace rtps */
} /* namespace eprosima */
