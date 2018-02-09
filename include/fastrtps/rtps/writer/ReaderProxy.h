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
 * @file ReaderProxy.h
 *
 */
#ifndef __RTPS_WRITER_READERPROXY_H__
#define __RTPS_WRITER_READERPROXY_H__
#ifndef DOXYGEN_SHOULD_SKIP_THIS_PUBLIC
#include <algorithm>
#include <mutex>
#include <set>
#include "../common/Types.h"
#include "../common/Locator.h"
#include "../common/SequenceNumber.h"
#include "../common/CacheChange.h"
#include "../common/FragmentNumber.h"
#include "../attributes/WriterAttributes.h"
#include "StatefulWriter.h"

#include <set>

//TODO(Ricardo) Move header to src/cpp

namespace eprosima {
namespace fastrtps {
namespace rtps {
class NackResponseDelay;
class NackSupressionDuration;
class InitialHeartbeat;


/**
 * ReaderProxy class that helps to keep the state of a specific Reader with respect to the RTPSWriter.
 * @ingroup WRITER_MODULE
 */
class ReaderProxy
{
    public:
        ~ReaderProxy();

        /**
         * Constructor.
         * @param rdata RemoteWriterAttributes to use in the creation.
         * @param times WriterTimes to use in the ReaderProxy.
         * @param SW Pointer to the StatefulWriter.
         */
        ReaderProxy(StatefulWriter::impl& writer, const RemoteReaderAttributes& rdata,
                const WriterTimes& times);

        void destroy_timers();

        void add_change(const ChangeForReader_t&);

        void back_to_live_change(const ChangeForReader_t&);

        size_t countChangesForReader() const;

        /**
         * Mark all changes up to the one indicated by the seqNum as Acknowledged.
         * If seqNum == 30, changes 1-29 are marked as ack.
         * @param seqNum Pointer to the seqNum
         */
        void acked_changes_set(const SequenceNumber_t& seqNum);

        /**
         * Mark all changes in the vector as requested.
         * @param seqNumSet Vector of sequenceNumbers
         * @return False if any change was set REQUESTED.
         */
        bool requested_changes_set(std::vector<SequenceNumber_t>& seqNumSet);

        /*!
         * @brief Lists all unsent changes. These changes are also relevants and valid.
         * @return STL vector with the unsent change list.
         */
        //TODO(Ricardo) Temporal
        //std::vector<const ChangeForReader_t*> get_unsent_changes() const;
        std::vector<ChangeForReader_t*> get_unsent_changes();
        /*!
         * @brief Lists all requested changes.
         * @return STL vector with the requested change list.
         */
        std::vector<const ChangeForReader_t*> get_requested_changes() const;

        /*!
         * @brief Sets a change to a particular status (if present in the ReaderProxy)
         * @param change change to search and set.
         * @param status Status to apply.
         */
        void set_change_to_status(const SequenceNumber_t& seq_num, ChangeForReaderStatus_t status);

        bool mark_fragment_as_sent_for_change(const SequenceNumber_t& sequence_number, FragmentNumber_t fragment);

        /*
         * Converts all changes with a given status to a different status.
         * @param previous Status to change.
         * @param next Status to adopt.
         */
        void convert_status_on_all_changes(ChangeForReaderStatus_t previous, ChangeForReaderStatus_t next);

        void setNotValid(const SequenceNumber_t& sequence_number);

        /*!
         * @brief Returns there is some UNACKNOWLEDGED change.
         * @return There is some UNACKNOWLEDGED change.
         */
        bool thereIsUnacknowledged() const;

        /**
         * Get a vector of all unacked changes by this Reader.
         * @param reqChanges Pointer to a vector of pointers.
         * @return True if correct.
         */
        bool unacked_changes(std::vector<const ChangeForReader_t*>* reqChanges);

        //!Attributes of the Remote Reader
        RemoteReaderAttributes m_att;

        //!Pointer to the associated StatefulWriter.
        StatefulWriter::impl& writer_;

        /*!
         * @brief Adds requested fragments. These fragments will be sent in next NackResponseDelay.
         * @param[in] frag_set set containing the requested fragments to be sent.
         * @param[in] sequence_number Sequence number to be paired with the requested fragments.
         * @return True if there is at least one requested fragment. False in other case.
         */
        bool requested_fragment_set(SequenceNumber_t sequence_number, const FragmentNumberSet_t& frag_set);

        /*!
         * @brief Returns the last NACKFRAG count.
         * @return Last NACKFRAG count.
         */
        uint32_t last_nackfrag_count() const { return last_nackfrag_count_; }

        uint32_t& last_nackfrag_count() { return last_nackfrag_count_; }

        /*!
         * @brief Sets the last NACKFRAG count.
         * @param lastNackfragCount New value for last NACKFRAG count.
         */
        void last_nackfrag_count(uint32_t last_nackfrag_count) { last_nackfrag_count_ = last_nackfrag_count; }

        //! Timed Event to manage the Acknack response delay.
        NackResponseDelay* mp_nackResponse;
        //! Timed Event to manage the delay to mark a change as UNACKED after sending it.
        NackSupressionDuration* mp_nackSupression;
        //! Timed Event to send initial heartbeat.
        InitialHeartbeat* mp_initialHeartbeat;
        //! Last ack/nack count
        uint32_t m_lastAcknackCount;

        /**
         * Filter a CacheChange_t, in this version always returns true.
         * @param change
         * @return
         */
        bool rtps_is_relevant(const CacheChange_t&){ return true; };

        SequenceNumber_t get_low_mark_nts() const { return changesFromRLowMark_; }

        //!Mutex
        std::recursive_mutex* mp_mutex;

        std::set<ChangeForReader_t, ChangeForReaderCmp> m_changesForReader;

        //!Set of the changes and its state.
    private:
        //! Last  NACKFRAG count.
        uint32_t last_nackfrag_count_;

        SequenceNumber_t changesFromRLowMark_;
};
}
} /* namespace rtps */
} /* namespace eprosima */
#endif
#endif /* __RTPS_WRITER_READERPROXY_H__ */
