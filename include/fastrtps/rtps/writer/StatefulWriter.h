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
 * @file StatefulWriter.h
 *
 */

#ifndef __RTPS_WRITER_STATEFULWRITER_H__
#define __RTPS_WRITER_STATEFULWRITER_H__
#ifndef DOXYGEN_SHOULD_SKIP_THIS_PUBLIC

#include "RTPSWriter.h"
#include "timedevent/PeriodicHeartbeat.h"
#include <condition_variable>
#include <mutex>

namespace eprosima
{
    namespace fastrtps
    {
        namespace rtps
        {
            class ReaderProxy;
            class CleanupEvent;

            /**
             * Class StatefulWriter, specialization of RTPSWriter that maintains information of each matched Reader.
             * @ingroup WRITER_MODULE
             */
            class StatefulWriter: public RTPSWriter
            {
                friend class RTPSParticipantImpl;
                //TODO(Ricardo) ReaderProxy?
                friend class ReaderProxy;
                friend class AsyncWriterThread;
                friend class InitialHeartbeat;
                friend class CleanupEvent;

                public:

                //!Destructor
                virtual ~StatefulWriter();

                //!Timed Event to manage the periodic HB to the Reader.
                // TODO Change to public because a bug. Refactor.
                PeriodicHeartbeat* mp_periodicHB;

                private:

                //!Constructor
                StatefulWriter(RTPSParticipantImpl*, GUID_t& guid, WriterAttributes& att,
                        WriterHistory& hist, WriterListener* listen = nullptr);

                //!Count of the sent heartbeats.
                Count_t m_heartbeatCount;

                //!WriterTimes
                WriterTimes m_times;

                //! Vector containin all the associated ReaderProxies.
                std::vector<ReaderProxy*> matched_readers_;

                //!EntityId used to send the HB.(only for builtin types performance)
                EntityId_t m_HBReaderEntityId;
                // TODO Join this mutex when main mutex would not be recursive.
                std::mutex all_acked_mutex_;
                std::condition_variable all_acked_cond_;
                // TODO Also remove when main mutex not recursive.
                bool all_acked_;

                /**
                 * Add a specific change to all ReaderLocators.
                 * @param p Pointer to the change.
                 */
                bool unsent_change_added_to_history(const CacheChange_t& change) override;

                /**
                 * Indicate the writer that a change has been removed by the history due to some HistoryQos requirement.
                 * @param a_change Pointer to the change that is going to be removed.
                 * @return True if removed correctly.
                 */
                bool change_removed_by_history(const SequenceNumber_t& sequence_number,
                        const InstanceHandle_t& handle) override;

                /**
                 * Method to indicate that there are changes not sent in some of all ReaderProxy.
                 */
                void send_any_unsent_changes();

                public:

                //!Increment the HB count.
                inline void incrementHBCount(){ ++m_heartbeatCount; };
                /**
                 * Add a matched reader.
                 * @param ratt Attributes of the reader to add.
                 * @return True if added.
                 */
                bool matched_reader_add(const RemoteReaderAttributes& ratt);
                /**
                 * Remove a matched reader.
                 * @param ratt Attributes of the reader to remove.
                 * @return True if removed.
                 */
                bool matched_reader_remove(const RemoteReaderAttributes& ratt);
                /**
                 * Tells us if a specific Reader is matched against this writer
                 * @param ratt Attributes of the reader to remove.
                 * @return True if it was matched.
                 */
                bool matched_reader_is_matched(const RemoteReaderAttributes& ratt);

                bool wait_for_all_acked(const Duration_t& max_wait);

                /**
                 * Update the Attributes of the Writer.
                 * @param att New attributes
                 */
                void updateAttributes(WriterAttributes& att);

                /** Get count of heartbeats
                 * @return count of heartbeats
                 */
                inline Count_t getHeartbeatCount() const {return this->m_heartbeatCount;};

                /** Get heartbeat reader entity id
                 * @return heartbeat reader entity id
                 */
                inline EntityId_t getHBReaderEntityId() {return this->m_HBReaderEntityId;};

                /**
                 * Get the RTPS participant
                 * @return RTPS participant
                 */
                inline RTPSParticipantImpl* getRTPSParticipant() const {return mp_RTPSParticipant;}

                /**
                 * Get the number of matched readers
                 * @return Number of the matched readers
                 */
                inline size_t getMatchedReadersSize() const {return matched_readers_.size();};

                /**
                 * Update the WriterTimes attributes of all associated ReaderProxy.
                 * @param times WriterTimes parameter.
                 */
                void updateTimes(WriterTimes& times);

                void add_flow_controller(std::unique_ptr<FlowController> controller);

                SequenceNumber_t next_sequence_number() const;

                void send_heartbeat(bool final = false);

                void send_heartbeat_to(const ReaderProxy& remote_reader, bool final = false);

                /*!
                 * @brief Sends a heartbeat to a remote reader.
                 * @remarks This function is non thread-safe.
                 */
                void send_heartbeat_to_nts(const ReaderProxy& remote_reader, SequenceNumber_t first_seq,
                        SequenceNumber_t last_seq, bool final = false);

                void process_acknack(const GUID_t reader_guid, uint32_t ack_count,
                        const SequenceNumberSet_t& sn_set, bool final_flag);

                void process_nackfrag(const GUID_t reader_guid, uint32_t nackfrag_count,
                        const SequenceNumber_t& writer_sn, const FragmentNumberSet_t& fn_set);

                private:

                void send_heartbeat_nts_(const std::vector<GUID_t>& remote_readers, const LocatorList_t& locators,
                        RTPSMessageGroup& message_group, SequenceNumber_t first_seq,
                        SequenceNumber_t last_seq, bool final = false);

                void check_acked_status_nts_();

                void cleanup_();

                bool disable_heartbeat_piggyback_;

                const uint32_t sendBufferSize_;

                int32_t currentUsageSendBufferSize_;

                std::vector<std::unique_ptr<FlowController> > m_controllers;

                CleanupEvent* cleanup_event_;

                StatefulWriter& operator=(const StatefulWriter&) = delete;
            };
        } /* namespace rtps */
    } /* namespace fastrtps */
} /* namespace eprosima */
#endif
#endif /* STATEFULWRITER_H_ */
