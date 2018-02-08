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
 * @file BuiltinProtocols.h
 *
 */

#ifndef __RTPS_BUILTIN_BUILTINPROTOCOLS_H__
#define __RTPS_BUILTIN_BUILTINPROTOCOLS_H__
#ifndef DOXYGEN_SHOULD_SKIP_THIS_PUBLIC

#include "../attributes/RTPSParticipantAttributes.h"
#include "../participant/RTPSParticipant.h"
#include "../reader/RTPSReader.h"
#include "../writer/RTPSWriter.h"


namespace eprosima {

namespace fastrtps{

    class WriterQos;
    class ReaderQos;
    class TopicAttributes;

namespace rtps {

class PDPSimple;

/**
 * Class BuiltinProtocols that contains builtin endpoints implementing the discovery and liveliness protocols.
 * *@ingroup BUILTIN_MODULE
 */
class BuiltinProtocols
{
    public:

    BuiltinProtocols(RTPSParticipant::impl& participant, const BuiltinAttributes& attributes);

    virtual ~BuiltinProtocols();

    /**
     * Initialize the builtin protocols.
     * @param attributes Discovery configuration attributes
     * @param p_part Pointer to the Participant implementation
     * @return True if correct.
     */
    bool init();

    /**
     * Update the metatraffic locatorlist after it was created. Because when you create the EDP readers you are not sure the selected endpoints can be used.
     * @param loclist LocatorList to update
     * @return True on success
     */
    bool updateMetatrafficLocators(LocatorList_t& loclist);

    //!BuiltinAttributes of the builtin protocols.
    BuiltinAttributes m_att;
    //!Pointer to the RTPSParticipantImpl.
    RTPSParticipant::impl& participant_;
    //!Pointer to the PDPSimple.
    PDPSimple* mp_PDP;
    //!Locator list for metatraffic
    LocatorList_t m_metatrafficMulticastLocatorList;
    //!Locator List for metatraffic unicast
    LocatorList_t m_metatrafficUnicastLocatorList;
    //! Initial peers
    LocatorList_t m_initialPeersList;

    /**
     * Add a local Writer to the BuiltinProtocols.
     * @param w Pointer to the RTPSWriter
     * @param topicAtt Attributes of the associated topic
     * @param wqos QoS policies dictated by the publisher
     * @return True if correct.
     */
    bool add_local_writer(RTPSWriter::impl& writer, TopicAttributes& topicAtt, WriterQos& wqos);
    /**
     * Add a local Reader to the BuiltinProtocols.
     * @param R Pointer to the RTPSReader.
     * @param topicAtt Attributes of the associated topic
     * @param rqos QoS policies dictated by the subscriber
     * @return True if correct.
     */
    bool add_local_reader(RTPSReader::impl& reader, TopicAttributes& topicAtt, ReaderQos& rqos);
    /**
     * Update a local Writer QOS
     * @param W Writer to update
     * @param wqos New Writer QoS
     * @return
     */
    bool update_local_writer(RTPSWriter::impl& writer, WriterQos& wqos);
    /**
     * Update a local Reader QOS
     * @param R Reader to update
     * @param qos New Reader QoS
     * @return
     */
    bool update_local_reader(RTPSReader::impl& reader, ReaderQos& qos);
    /**
     * Remove a local Writer from the builtinProtocols.
     * @param W Pointer to the writer.
     * @return True if correctly removed.
     */
    bool remove_local_writer(RTPSWriter::impl& writer);
    /**
     * Remove a local Reader from the builtinProtocols.
     * @param R Pointer to the reader.
     * @return True if correctly removed.
     */
    bool remove_local_reader(RTPSReader::impl& reader);

    //! Announce RTPSParticipantState (force the sending of a DPD message.)
    void announceRTPSParticipantState();
    //!Stop the RTPSParticipant Announcement (used in tests to avoid multiple packets being send)
    void stopRTPSParticipantAnnouncement();
    //!Reset to timer to make periodic RTPSParticipant Announcements.
    void resetRTPSParticipantAnnouncement();

};

} // namespace rtps
} // namespace fastrtps
} // namespace eprosima
#endif
#endif // __RTPS_BUILTIN_BUILTINPROTOCOLS_H__
