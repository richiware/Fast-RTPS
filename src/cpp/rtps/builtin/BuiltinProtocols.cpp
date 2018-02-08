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
 * @file BuiltinProtocols.cpp
 *
 */

#include <fastrtps/rtps/builtin/BuiltinProtocols.h>
#include <fastrtps/rtps/common/Locator.h>
#include <fastrtps/rtps/builtin/discovery/participant/PDPSimple.h>
#include <fastrtps/rtps/builtin/discovery/endpoint/EDP.h>
#include <fastrtps/rtps/builtin/liveliness/WLP.h>
#include "../participant/RTPSParticipantImpl.h"
#include "../reader/RTPSReaderImpl.h"
#include "../writer/RTPSWriterImpl.h"
#include <fastrtps/log/Log.h>
#include <fastrtps/utils/IPFinder.h>

#include <algorithm>



using namespace eprosima::fastrtps;

namespace eprosima {
namespace fastrtps{
namespace rtps {


BuiltinProtocols::BuiltinProtocols(RTPSParticipant::impl& participant, const BuiltinAttributes& attributes):
    m_att(attributes), participant_(participant),
    mp_PDP(nullptr)
{
}

BuiltinProtocols::~BuiltinProtocols()
{
    // Send participant is disposed
    if(mp_PDP != nullptr)
    {
        mp_PDP->announceParticipantState(true, true);
    }
    if(mp_PDP != nullptr)
    {
        delete(mp_PDP);
    }
}


bool BuiltinProtocols::init()
{
    m_metatrafficUnicastLocatorList = m_att.metatrafficUnicastLocatorList;
    m_metatrafficMulticastLocatorList = m_att.metatrafficMulticastLocatorList;
    m_initialPeersList = m_att.initialPeersList;

    if(m_att.use_SIMPLE_RTPSParticipantDiscoveryProtocol)
    {
        mp_PDP = new PDPSimple(this, participant_); //TODO(Ricardo) Change
        if(!mp_PDP->init())
        {
            logError(RTPS_PDP,"Participant discovery configuration failed");
            return false;
        }
        mp_PDP->announceParticipantState(true);
        mp_PDP->resetParticipantAnnouncement();
    }

    return true;
}

bool BuiltinProtocols::updateMetatrafficLocators(LocatorList_t& loclist)
{
    m_metatrafficUnicastLocatorList = loclist;
    return true;
}

bool BuiltinProtocols::add_local_writer(RTPSWriter::impl& writer, fastrtps::TopicAttributes& topicAtt,
        fastrtps::WriterQos& wqos)
{
    bool ok = false;
    if(mp_PDP!=nullptr)
    {
        ok |= mp_PDP->getEDP()->newLocalWriterProxyData(writer, topicAtt, wqos);

        if(mp_PDP->wlp() !=nullptr)
        {
            ok|= mp_PDP->wlp()->addLocalWriter(writer, wqos);
        }
        else
        {
            logWarning(RTPS_LIVELINESS, "LIVELINESS is not used in this Participant, register a Writer is impossible");
        }
    }
    else
    {
        logWarning(RTPS_EDP, "EDP is not used in this Participant, register a Writer is impossible");
    }
    return ok;
}

bool BuiltinProtocols::add_local_reader(RTPSReader::impl& reader,fastrtps::TopicAttributes& topicAtt, fastrtps::ReaderQos& rqos)
{
    bool ok = false;
    if(mp_PDP!=nullptr)
    {
        ok |= mp_PDP->getEDP()->newLocalReaderProxyData(reader,topicAtt, rqos);
    }
    else
    {
        logWarning(RTPS_EDP, "EDP is not used in this Participant, register a Reader is impossible");
    }
    return ok;
}

bool BuiltinProtocols::update_local_writer(RTPSWriter::impl& writer, WriterQos& wqos)
{
    bool ok = false;
    if(mp_PDP!=nullptr && mp_PDP->getEDP()!=nullptr)
    {
        ok |= mp_PDP->getEDP()->updatedLocalWriter(writer, wqos);
    }
    if(mp_PDP->wlp() != nullptr)
    {
        ok |= mp_PDP->wlp()->updateLocalWriter(writer, wqos);
    }
    return ok;
}

bool BuiltinProtocols::update_local_reader(RTPSReader::impl& reader, ReaderQos& rqos)
{
    bool ok = false;
    if(mp_PDP!=nullptr && mp_PDP->getEDP()!=nullptr)
    {
        ok |= mp_PDP->getEDP()->updatedLocalReader(reader, rqos);
    }
    return ok;
}

bool BuiltinProtocols::remove_local_writer(RTPSWriter::impl& writer)
{
    bool ok = false;
    if(mp_PDP->wlp() != nullptr)
    {
        ok|= mp_PDP->wlp()->removeLocalWriter(writer);
    }
    if(mp_PDP!=nullptr && mp_PDP->getEDP() != nullptr)
    {
        ok|= mp_PDP->getEDP()->removeLocalWriter(writer);
    }
    return ok;
}

bool BuiltinProtocols::remove_local_reader(RTPSReader::impl& reader)
{
    bool ok = false;
    if(mp_PDP!=nullptr && mp_PDP->getEDP() != nullptr)
    {
        ok|= mp_PDP->getEDP()->removeLocalReader(reader);
    }
    return ok;
}

void BuiltinProtocols::announceRTPSParticipantState()
{
    mp_PDP->announceParticipantState(false);
}

void BuiltinProtocols::stopRTPSParticipantAnnouncement()
{
    mp_PDP->stopParticipantAnnouncement();
}

void BuiltinProtocols::resetRTPSParticipantAnnouncement()
{
    mp_PDP->resetParticipantAnnouncement();
}

}
} /* namespace rtps */
} /* namespace eprosima */
