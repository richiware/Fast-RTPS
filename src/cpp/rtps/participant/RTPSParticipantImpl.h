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
 * @file RTPSParticipantImpl.h
 */

#ifndef __RTPS_PARTICIPANT_RTPSPARTICIPANTIMPL_H__
#define __RTPS_PARTICIPANT_RTPSPARTICIPANTIMPL_H__
#ifndef DOXYGEN_SHOULD_SKIP_THIS_PUBLIC

#include <stdio.h>
#include <stdlib.h>
#include <list>
#include <sys/types.h>
#include <mutex>
#include <atomic>
#include <fastrtps/utils/Semaphore.h>

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

#include <fastrtps/rtps/participant/RTPSParticipant.h>
#include <fastrtps/rtps/attributes/RTPSParticipantAttributes.h>
#include <fastrtps/rtps/participant/RTPSParticipantDiscoveryInfo.h>
#include <fastrtps/rtps/common/Guid.h>
#include <fastrtps/rtps/builtin/discovery/endpoint/EDPSimple.h>
#include <fastrtps/rtps/builtin/data/ReaderProxyData.h>
#include <fastrtps/rtps/builtin/data/WriterProxyData.h>
#include <fastrtps/rtps/network/NetworkFactory.h>
#include <fastrtps/rtps/network/ReceiverResource.h>
#include <fastrtps/rtps/network/SenderResource.h>
#include <fastrtps/rtps/messages/MessageReceiver.h>
#include "../writer/RTPSWriterImpl.h"
#include "../reader/RTPSReaderImpl.h"
#include <fastrtps/rtps/history/WriterHistory.h>

#if HAVE_SECURITY
#include "../security/SecurityManager.h"
#endif

#include <memory>

namespace eprosima {
namespace fastrtps{

class WriterQos;
class ReaderQos;
class TopicAttributes;
class MessageReceiver;

namespace rtps {

class RTPSParticipant;
class ResourceEvent;
class AsyncWriterThread;
class BuiltinProtocols;
struct CDRMessage_t;
class Endpoint;
class WriterAttributes;
class WriterListener;
class ReaderAttributes;
class ReaderHistory;
class ReaderListener;
class StatefulReader;
class PDPSimple;
class FlowController;

/*
   Receiver Control block is a struct we use to encapsulate the resources that take part in message reception.
   It contains:
   -A ReceiverResource (as produced by the NetworkFactory Element)
   -A list of associated wirters and readers
   -A buffer for message storage
   -A mutex for the lists
   The idea is to create the thread that performs blocking calllto ReceiverResource.Receive and processes the message
   from the Receiver, so the Transport Layer does not need to be aware of the existence of what is using it.

*/
typedef struct ReceiverControlBlock
{
    ReceiverResource Receiver;
    MessageReceiver* mp_receiver; //Associated Readers/Writers inside of MessageReceiver
    std::thread* m_thread;
    std::atomic<bool> resourceAlive;
    ReceiverControlBlock(ReceiverResource&& rec):Receiver(std::move(rec)), mp_receiver(nullptr), m_thread(nullptr), resourceAlive(true)
    {
    }
    ReceiverControlBlock(ReceiverControlBlock&& origen):Receiver(std::move(origen.Receiver)), mp_receiver(origen.mp_receiver), m_thread(origen.m_thread), resourceAlive(true)
    {
        origen.m_thread = nullptr;
        origen.mp_receiver = nullptr;
    }

    private:
    ReceiverControlBlock(const ReceiverControlBlock&) = delete;
    const ReceiverControlBlock& operator=(const ReceiverControlBlock&) = delete;

} ReceiverControlBlock;


/**
 * @brief Class RTPSParticipantImpl, it contains the private implementation of the RTPSParticipant functions and allows the creation and removal of writers and readers. It manages the send and receive threads.
 * @ingroup RTPS_MODULE
 */
class RTPSParticipant::impl
{
    public:

        class Listener
        {
            public:

                Listener() = default;

                virtual ~Listener() = default;

                /**
                 * This method is invoked when a new participant is discovered
                 * @param part Discovered participant
                 * @param info Discovery information of the participant
                 */
                virtual void onRTPSParticipantDiscovery(RTPSParticipant::impl&, const RTPSParticipantDiscoveryInfo&) {}

#if HAVE_SECURITY
                virtual void onRTPSParticipantAuthentication(RTPSParticipant::impl&,
                        const RTPSParticipantAuthenticationInfo&) {}
#endif
        };

        /**
         * @param param
         * @param guidP
         * @param part
         * @param plisten
         */
        impl(const RTPSParticipantAttributes &param, const GuidPrefix_t& guid,
                Listener* listener = nullptr);

        ~impl();

        /**
         * Get associated GUID
         * @return Associated GUID
         */
        inline const GUID_t& guid() const { return guid_; }

        void guid(GUID_t& guid) { guid_ = guid; }

        //! Announce RTPSParticipantState (force the sending of a DPD message.)
        void announceRTPSParticipantState();

        //!Stop the RTPSParticipant Announcement (used in tests to avoid multiple packets being send)
        void stopRTPSParticipantAnnouncement();

        //!Reset to timer to make periodic RTPSParticipant Announcements.
        void resetRTPSParticipantAnnouncement();

        void loose_next_change();

        /**
         * Activate a Remote Endpoint defined in the Static Discovery.
         * @param pguid GUID_t of the endpoint.
         * @param userDefinedId userDeinfed Id of the endpoint.
         * @param kind kind of endpoint
         * @return True if correct.
         */
        bool newRemoteEndpointDiscovered(const GUID_t& pguid, int16_t userDefinedId,EndpointKind_t kind);

        /**
         * Assert the liveliness of a remote participant
         * @param guidP GuidPrefix_t of the participant.
         */
        void assertRemoteRTPSParticipantLiveliness(const GuidPrefix_t& guidP);

        /**
         * Get the RTPSParticipant ID
         * @return RTPSParticipant ID
         */
        inline uint32_t getRTPSParticipantID() const { return (uint32_t)att_.participantID;};

        //!Post to the resource semaphore
        void ResourceSemaphorePost();

        //!Wait for the resource semaphore
        void ResourceSemaphoreWait();

        //!Get Pointer to the Event Resource.
        ResourceEvent& getEventResource();

        //!Send Method - Deprecated - Stays here for reference purposes
        void sendSync(CDRMessage_t* msg, Endpoint *pend, const Locator_t& destination_loc);

        //!Get the participant Mutex
        std::recursive_mutex* getParticipantMutex() const { return mutex_; };

        /**
         * Get the participant listener
         * @return participant listener
         */
        inline Listener* listener() { return listener_; }

        /**
         * Get a a pair of pointer to the RTPSReaders used by SimpleEDP for discovery of Publisher and Subscribers
         * @return std::pair of pointers to StatefulReaders where the first on points to the SubReader and the second to PubReader.
         */
        std::pair<StatefulReader*,StatefulReader*> getEDPReaders();

        std::vector<std::string> getParticipantNames() const;

        std::vector<std::unique_ptr<FlowController>>& getFlowControllers() { return controllers_;}

        /*!
         * @remarks Non thread-safe.
         */
        const std::vector<std::shared_ptr<RTPSWriter::impl>>& getAllWriters() const;

        /*!
         * @remarks Non thread-safe.
         */
        const std::vector<std::shared_ptr<RTPSReader::impl>>& getAllReaders() const;

        uint32_t getMaxMessageSize() const;

        uint32_t getMaxDataSize();

        uint32_t calculateMaxDataSize(uint32_t length);

#if HAVE_SECURITY
        security::SecurityManager& security_manager() { return security_manager_; }

        bool is_rtps_protected() const { return is_rtps_protected_; }
#endif

        PDPSimple* pdpsimple();

        bool get_remote_writer_info(const GUID_t& writerGuid, WriterProxyData& returnedInfo);

        bool get_remote_reader_info(const GUID_t& readerGuid, ReaderProxyData& returnedInfo);

        NetworkFactory& network_factory() { return network_factory_; }

        uint32_t get_min_network_send_buffer_size() { return network_factory_.get_min_send_buffer_size(); }

        const RTPSParticipantAttributes& getRTPSParticipantAttributes() const
        {
            return att_;
        }

        /**
         * Create a Writer in this RTPSParticipant.
         * @param Writer Pointer to pointer of the Writer, used as output. Only valid if return==true.
         * @param param WriterAttributes to define the Writer.
         * @param entityId EntityId assigned to the Writer.
         * @param isBuiltin Bool value indicating if the Writer is builtin (Discovery or Liveliness protocol) or is created for the end user.
         * @return True if the Writer was correctly created.
         */
         //TODO(Ricardo) Change WriterHistory to impl. first change api statelesswriter and statefulwriter
        std::shared_ptr<RTPSWriter::impl> create_writer(const WriterAttributes& param,
                WriterHistory::impl& history, RTPSWriter::impl::Listener* listener,
                const EntityId_t& entityId = c_EntityId_Unknown, bool isBuiltin = false);

        /**
         * Create a Reader in this RTPSParticipant.
         * @param Reader Pointer to pointer of the Reader, used as output. Only valid if return==true.
         * @param param ReaderAttributes to define the Reader.
         * @param entityId EntityId assigned to the Reader.
         * @param isBuiltin Bool value indicating if the Reader is builtin (Discovery or Liveliness protocol) or is created for the end user.
         * @return True if the Reader was correctly created.
         */
        std::shared_ptr<RTPSReader::impl> create_reader(const ReaderAttributes& param,
                ReaderHistory* hist, RTPSReader::impl::Listener* listen,
                const EntityId_t& entityId = c_EntityId_Unknown, bool isBuiltin = false, bool enable = true);

        bool enable_reader(RTPSReader::impl& reader);

        /**
         * Register a Writer in the BuiltinProtocols.
         * @param Writer Pointer to the RTPSWriter.
         * @param topicAtt TopicAttributes of the Writer.
         * @param wqos WriterQos.
         * @return True if correctly registered.
         */
        bool register_writer(RTPSWriter::impl& writer, TopicAttributes& topicAtt, WriterQos& wqos);

        /**
         * Register a Reader in the BuiltinProtocols.
         * @param Reader Pointer to the RTPSReader.
         * @param topicAtt TopicAttributes of the Reader.
         * @param rqos ReaderQos.
         * @return  True if correctly registered.
         */
        bool register_reader(RTPSReader::impl& reader, TopicAttributes& topicAtt, ReaderQos& rqos);

        /**
         * Update local writer QoS
         * @param Writer Writer to update
         * @param wqos New QoS for the writer
         * @return True on success
         */
        bool update_local_writer(RTPSWriter::impl& writer, WriterQos& wqos);

        /**
         * Update local reader QoS
         * @param Reader Reader to update
         * @param rqos New QoS for the reader
         * @return True on success
         */
        bool update_local_reader(RTPSReader::impl& reader, ReaderQos& rqos);



        /**
         * Get the participant attributes
         * @return Participant attributes
         */
        inline RTPSParticipantAttributes& getAttributes() {return att_;};

        /**
         * Get the begin of the user reader list
         * @return Iterator pointing to the begin of the user reader list
         */
        std::vector<RTPSReader::impl*>::iterator userReadersListBegin(){return user_reader_list_.begin();};

        /**
         * Get the end of the user reader list
         * @return Iterator pointing to the end of the user reader list
         */
        std::vector<RTPSReader::impl*>::iterator userReadersListEnd(){return user_reader_list_.end();};

        /**
         * Get the begin of the user writer list
         * @return Iterator pointing to the begin of the user writer list
         */
        std::vector<RTPSWriter::impl*>::iterator userWritersListBegin(){return user_writer_list_.begin();};

        /**
         * Get the end of the user writer list
         * @return Iterator pointing to the end of the user writer list
         */
        std::vector<RTPSWriter::impl*>::iterator userWritersListEnd(){return user_writer_list_.end();};

        /** Helper function that creates ReceiverResources based on a Locator_t List, possibly mutating
          some and updating the list. DOES NOT associate endpoints with it.
          @param Locator_list - Locator list to be used to create the ReceiverResources
          @param ApplyMutation - True if we want to create a Resource with a "similar" locator if the one we provide is unavailable
          */
        static const int MutationTries = 100;

        void createReceiverResources(LocatorList_t& Locator_list, bool ApplyMutation);

        bool networkFactoryHasRegisteredTransports() const;

        bool remove_writer(std::shared_ptr<RTPSWriter::impl>& writer);

        bool remove_reader(std::shared_ptr<RTPSReader::impl>& reader);

#if HAVE_SECURITY
        void set_endpoint_rtps_protection_supports(Endpoint* endpoint, bool support);
#endif

    private:

        //!Attributes of the RTPSParticipant
        RTPSParticipantAttributes att_;

        //!Guid of the RTPSParticipant.
        GUID_t guid_;

        //! Event Resource
        ResourceEvent* event_thread_;

        //! BuiltinProtocols of this RTPSParticipant
        BuiltinProtocols* builtin_protocols_;

        //!Semaphore to wait for the listen thread creation.
        Semaphore* resources_semaphore_;

        //!Id counter to correctly assign the ids to writers and readers.
        uint32_t id_counter_;

        //!Writer List.
        std::vector<std::shared_ptr<RTPSWriter::impl>> all_writer_list_;

        //!Reader List
        std::vector<std::shared_ptr<RTPSReader::impl>> all_reader_list_;

        //!Listen thread list.
        //!Writer List.
        std::vector<RTPSWriter::impl*> user_writer_list_;

        //!Reader List
        std::vector<RTPSReader::impl*> user_reader_list_;

        //!Network Factory
        NetworkFactory network_factory_;

#if HAVE_SECURITY
        // Security manager
        security::SecurityManager security_manager_;
#endif

        //! Encapsulates all associated resources on a Receiving element.
        std::list<ReceiverControlBlock> receiver_resource_list_;

        //! Receiver resource list needs its own mutext to avoid a race condition.
        std::mutex receiver_resource_list_mutex_;

        //!SenderResource List
        std::mutex send_resources_mutex_;

        std::vector<SenderResource> sender_resource_;

        //!Participant Listener
        Listener* listener_;

        impl& operator=(const impl&) = delete;

        /**
         * Method to check if a specific entityId already exists in this RTPSParticipant
         * @param ent EnityId to check
         * @param kind Endpoint Kind.
         * @return True if exists.
         */
        bool existsEntityId(const EntityId_t& ent,EndpointKind_t kind) const;

        /**
         * Assign an endpoint to the ReceiverResources, based on its LocatorLists.
         * @param endp Pointer to the endpoint.
         * @return True if correct.
         */
        //TODO(Ricardo) Review if necessary the separation.
        bool assign_endpoint_to_locatorlist(Endpoint& endpoint, LocatorList_t& list);

        bool assign_endpoint_listen_resources(Endpoint& endpoint);

        /** Create the new ReceiverResources needed for a new Locator, contains the calls to assignEndpointListenResources
          and consequently assignEndpoint2LocatorList
          @param pend - Pointer to the endpoint which triggered the creation of the Receivers
          */
        bool createAndAssociateReceiverswithEndpoint(Endpoint& pend);

        /** Function to be called from a new thread, which takes cares of performing a blocking receive
          operation on the ReceiveResource
          @param buffer - Position of the buffer we use to store data
          @param locator - Locator that triggered the creation of the resource
          */
        void performListenOperation(ReceiverControlBlock *receiver, Locator_t input_locator);

        /** Create non-existent SendResources based on the Locator list of the entity
          @param pend - Pointer to the endpoint whose SenderResources are to be created
          */
        bool createSendResources(Endpoint *pend);

        /** When we want to create a new Resource but the physical channel specified by the Locator
          can not be opened, we want to mutate the Locator to open a more or less equivalent channel.
          @param loc -  Locator we want to change
          */
        Locator_t applyLocatorAdaptRule(Locator_t loc);

        //!Participant Mutex
        std::recursive_mutex* mutex_;

        /*
         * Flow controllers for this participant.
         */
        std::vector<std::unique_ptr<FlowController> > controllers_;

#if HAVE_SECURITY
        bool is_rtps_protected_;
#endif

        //TODO(Ricardo)Remove
        static uint32_t m_maxRTPSParticipantID;
};

inline RTPSParticipant::impl& get_implementation(RTPSParticipant& participant) { return *participant.impl_; }

} //namespace rtps
} //namespace fastrtps
} //namespace eprosima

#endif
#endif /* __RTPS_PARTICIPANT_RTPSPARTICIPANTIMPL_H__ */
