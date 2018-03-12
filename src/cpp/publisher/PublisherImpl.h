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
 * @file Publisher.h
 */



#ifndef __PUBLISHER_PUBLISHERIMPL_H__
#define __PUBLISHER_PUBLISHERIMPL_H__
#ifndef DOXYGEN_SHOULD_SKIP_THIS_PUBLIC

#include <fastrtps/publisher/Publisher.h>
#include <fastrtps/participant/Participant.h>
#include <fastrtps/rtps/common/Locator.h>
#include <fastrtps/rtps/common/Guid.h>
#include <fastrtps/attributes/PublisherAttributes.h>
#include <fastrtps/rtps/writer/WriterListener.h>
#include "../rtps/history/WriterHistoryImpl.h"
#include "../rtps/writer/RTPSWriterImpl.h"

#include <map>
#include <mutex>

namespace eprosima {
namespace fastrtps{

class TopicDataType;

/**
 * Class PublisherImpl, contains the actual implementation of the behaviour of the Publisher.
 * @ingroup FASTRTPS_MODULE
 */
class Publisher::impl
{
    using map = std::map<rtps::InstanceHandle_t, std::set<rtps::SequenceNumber_t>>;

    public:

        class Listener
        {
            public:

                Listener() = default;

                virtual ~Listener() = default;

                virtual void onPublicationMatched(Publisher::impl&, const rtps::MatchingInfo&) {}
        };

        /**
         * Create a publisher, assigning its pointer to the associated writer.
         * Don't use directly, create Publisher using DomainRTPSParticipant static function.
         */
        impl(Participant::impl& participant, TopicDataType* type,
                const PublisherAttributes& att, Listener* listener = nullptr);

        virtual ~impl();

        bool init();

        void deinit();

        bool enable();

        /**
         * 
         * @param kind
         * @param  Data
         * @return
         */
        bool create_new_change(rtps::ChangeKind_t change_kind, void* data);

        /**
         * 
         * @param kind
         * @param  Data
         * @param wparams
         * @return
         */
        bool create_new_change_with_params(rtps::ChangeKind_t change_kind, void* data, rtps::WriteParams &wparams);

        /**
         * Removes the cache change with the minimum sequence number
         * @return True if correct.
         */
        bool removeMinSeqChange();
        /**
         * Removes all changes from the History.
         * @param[out] removed Number of removed elements
         * @return True if correct.
         */
        bool removeAllChange(size_t* removed);

        /**
         * Get the number of elements in the History.
         * @return Number of elements in the History.
         */
        size_t getHistoryElementsNumber();

        /**
         * 
         * @return
         */
        const rtps::GUID_t& getGuid();

        /**
         * Update the Attributes of the publisher;
         * @param att Reference to a PublisherAttributes object to update the parameters;
         * @return True if correctly updated, false if ANY of the updated parameters cannot be updated
         */
        bool updateAttributes(PublisherAttributes& att);

        /**
         * Get the Attributes of the Subscriber.
         * @return Attributes of the Subscriber.
         */
        inline const PublisherAttributes& getAttributes() const { return att_; };

        /**
         * Get topic data type
         * @return Topic data type
         */
        TopicDataType* getType() {return type_;};

        bool try_remove_change(std::unique_lock<std::recursive_mutex>& lock);

        bool wait_for_all_acked(const rtps::Time_t& max_wait);

        Participant::impl& participant() { return participant_; }

    private:

        bool unsent_change_added_to_history_nts_(const rtps::CacheChange_t&);

        /**
         * Indicate the writer that a change has been removed by the history due to some HistoryQos requirement.
         * @param a_change Pointer to the change that is going to be removed.
         * @return True if removed correctly.
         */
        bool change_removed_by_history_(const rtps::SequenceNumber_t& sequence_number,
                const rtps::InstanceHandle_t& handle);

#if defined(__DEBUG)
        int get_mutex_owner_() const;

        int get_thread_id_() const;
#endif

        Participant::impl& participant_;

        //TODO Make private again

        //! Pointer to the associated Data Writer.
        std::shared_ptr<rtps::RTPSWriter::impl> writer_;

        //! Pointer to the TopicDataType object.
        TopicDataType* type_;

        //!Attributes of the Publisher
        PublisherAttributes att_;

        rtps::WriterHistory::impl history_;

        Listener* listener_;

        //!Listener to capture the events of the Writer
        class PublisherWriterListener: public rtps::RTPSWriter::impl::Listener
        {
            public:
                    PublisherWriterListener(Publisher::impl& publisher) : publisher_(publisher) {}

                    virtual ~PublisherWriterListener() = default;

                    void onWriterMatched(rtps::RTPSWriter::impl& writer, const rtps::MatchingInfo& info) override;

                    Publisher::impl& publisher_;
        } writer_listener_;

        uint32_t high_mark_for_frag_;

        map changes_by_instance_;

#if defined(__DEBUG)
        mutable std::mutex mutex_;
#else
        std::mutex mutex_;
#endif
};

inline Publisher::impl& get_implementation(Publisher& publisher) { return *publisher.impl_; }

inline const Publisher::impl& get_implementation(const Publisher& publisher) { return *publisher.impl_; }

} /* namespace  */
} /* namespace eprosima */
#endif
#endif /* __PUBLISHER_PUBLISHERIMPL_H__*/
