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

/*
 * RTPSDomain.cpp
 *
 */

#include <fastrtps/rtps/RTPSDomain.h>
#include "participant/RTPSParticipantImpl.h"
#include <fastrtps/log/Log.h>
#include <fastrtps/transport/UDPv4Transport.h>
#include <fastrtps/transport/UDPv6Transport.h>
#include <fastrtps/transport/test_UDPv4Transport.h>
#include <fastrtps/utils/IPFinder.h>
#include <fastrtps/utils/eClock.h>
#include <fastrtps/rtps/writer/RTPSWriter.h>
#include "history/WriterHistoryImpl.h"
#include <fastrtps/rtps/reader/RTPSReader.h>

namespace eprosima {
namespace fastrtps{
namespace rtps {

uint32_t RTPSDomain::m_maxRTPSParticipantID = 0;
std::vector<RTPSParticipant*> RTPSDomain::m_RTPSParticipants;
std::set<uint32_t> RTPSDomain::m_RTPSParticipantIDs;

RTPSDomain::RTPSDomain()
{
    srand (static_cast <unsigned> (time(0)));
}

RTPSDomain::~RTPSDomain()
{

}

void RTPSDomain::stopAll()
{
    logInfo(RTPS_PARTICIPANT,"DELETING ALL ENDPOINTS IN THIS DOMAIN");

    while(!m_RTPSParticipants.empty())
    {
        RTPSDomain::remove_participant(*m_RTPSParticipants.begin());
    }

    logInfo(RTPS_PARTICIPANT,"RTPSParticipants deleted correctly ");
    eClock::my_sleep(100);
}

RTPSParticipant* RTPSDomain::create_participant(RTPSParticipantAttributes& PParam,
        RTPSParticipantListener* listen)
{
    logInfo(RTPS_PARTICIPANT,"");

    if(PParam.builtin.leaseDuration < c_TimeInfinite && PParam.builtin.leaseDuration <= PParam.builtin.leaseDuration_announcementperiod) //TODO CHeckear si puedo ser infinito
    {
        logError(RTPS_PARTICIPANT,"RTPSParticipant Attributes: LeaseDuration should be >= leaseDuration announcement period");
        return nullptr;
    }
    if(PParam.use_IP4_to_send == false && PParam.use_IP6_to_send == false)
    {
        logError(RTPS_PARTICIPANT,"Use IP4 OR User IP6 to send must be set to true");
        return nullptr;
    }
    if(!PParam.defaultUnicastLocatorList.isValid())
    {
        logError(RTPS_PARTICIPANT,"Default Unicast Locator List contains invalid Locator");
        return nullptr;
    }
    if(!PParam.defaultMulticastLocatorList.isValid())
    {
        logError(RTPS_PARTICIPANT,"Default Multicast Locator List contains invalid Locator");
        return nullptr;
    }


    RTPSParticipant* participant = new RTPSParticipant(PParam, GuidPrefix_t::unknown(), listen);

    if(participant != nullptr)
    {
        // Check there is at least one transport registered.
        if(!get_implementation(*participant).networkFactoryHasRegisteredTransports())
        {
            logError(RTPS_PARTICIPANT,"Cannot create participant, because there is any transport");
            return nullptr;
        }

        m_RTPSParticipants.push_back(participant);
        return participant;
    }

    return nullptr;
}

bool RTPSDomain::remove_participant(RTPSParticipant* participant)
{
    if(participant != nullptr)
    {
        for(auto it = m_RTPSParticipants.begin(); it != m_RTPSParticipants.end(); ++it)
        {
            if((*it)->guid().guidPrefix == participant->guid().guidPrefix)
            {
                m_RTPSParticipantIDs.erase(m_RTPSParticipantIDs.find((*it)->getRTPSParticipantID()));
                delete *it;
                m_RTPSParticipants.erase(it);
                return true;
            }
        }
    }

    logError(RTPS_PARTICIPANT,"RTPSParticipant not valid or not recognized");
    return false;
}

RTPSWriter* RTPSDomain::createRTPSWriter(RTPSParticipant* participant, WriterAttributes& watt, WriterHistory& hist,
        WriterListener* listen)
{
    for(auto it= m_RTPSParticipants.begin();it!=m_RTPSParticipants.end();++it)
    {
        if((*it)->guid().guidPrefix == participant->guid().guidPrefix)
        {
            //TODO(Ricardo) Remove
            return nullptr;
        }
    }

    return nullptr;
}

bool RTPSDomain::removeRTPSWriter(RTPSWriter* writer)
{
    if(writer!=nullptr)
    {
        for(auto it= m_RTPSParticipants.begin();it!=m_RTPSParticipants.end();++it)
        {
            //if((*it)->guid().guidPrefix == writer->guid().guidPrefix)
            {
                //TODO(Ricardo) Remove
                //return (*it)->deleteUserEndpoint((Endpoint*)writer);
                return false;
            }
        }
    }

    return false;
}

RTPSReader* RTPSDomain::createRTPSReader(RTPSParticipant* participant, ReaderAttributes& ratt,
        ReaderHistory* rhist, ReaderListener* rlisten)
{
    for(auto it= m_RTPSParticipants.begin();it!=m_RTPSParticipants.end();++it)
    {
        //if((*it)->guid().guidPrefix == participant->getGuid().guidPrefix)
        {
            return nullptr;
        }
    }

    return nullptr;
}

bool RTPSDomain::removeRTPSReader(RTPSReader* reader)
{
    if(reader !=  nullptr)
    {
        for(auto it= m_RTPSParticipants.begin();it!=m_RTPSParticipants.end();++it)
        {
            //if((*it)->guid().guidPrefix == reader->getGuid().guidPrefix)
            {
                //return (*it)->deleteUserEndpoint((Endpoint*)reader);
            }
        }
    }

    return false;
}

} /* namespace  rtps */
} /* namespace  fastrtps */
} /* namespace eprosima */



