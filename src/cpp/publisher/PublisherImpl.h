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

#include <map>
#include <mutex>

namespace eprosima {
namespace fastrtps{
namespace rtps
{
class RTPSWriter;
class RTPSParticipant;
}

class TopicDataType;
class PublisherListener;


/**
 * Class PublisherImpl, contains the actual implementation of the behaviour of the Publisher.
 * @ingroup FASTRTPS_MODULE
 */
class Publisher::impl
{
    using map = std::map<InstanceHandle_t, std::set<SequenceNumber_t>>;

    public:

    /**
     * Create a publisher, assigning its pointer to the associated writer.
     * Don't use directly, create Publisher using DomainRTPSParticipant static function.
     */
    impl(Participant::impl& p, Publisher& publisher, TopicDataType* ptype,
            PublisherAttributes& att, PublisherListener* p_listen = nullptr);

    virtual ~impl();

    /**
     * 
     * @param kind
     * @param  Data
     * @return
     */
    bool create_new_change(ChangeKind_t change_kind, void* data);

    /**
     * 
     * @param kind
     * @param  Data
     * @param wparams
     * @return
     */
    bool create_new_change_with_params(ChangeKind_t change_kind, void* data, WriteParams &wparams);

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
    const GUID_t& getGuid();

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
    inline const PublisherAttributes& getAttributes(){ return att_; };

    /**
     * Get topic data type
     * @return Topic data type
     */
    TopicDataType* getType() {return type_;};

    bool try_remove_change(std::unique_lock<std::recursive_mutex>& lock);

    bool wait_for_all_acked(const Time_t& max_wait);

    private:

    Participant::impl& participant_;

    //TODO Make private again
    public:
    //! Pointer to the associated Data Writer.
    RTPSWriter* writer_;

    //! Pointer to the TopicDataType object.
    TopicDataType* type_;

    //!Attributes of the Publisher
    PublisherAttributes att_;

    WriterHistory history_;

    //!PublisherListener
    PublisherListener* listener_;

    //!Listener to capture the events of the Writer
    class PublisherWriterListener: public WriterListener
    {
        public:
            PublisherWriterListener(Publisher& publisher): publisher_(publisher){};

            virtual ~PublisherWriterListener(){};

            void onWriterMatched(RTPSWriter* writer,MatchingInfo& info);

            Publisher& publisher_;
    } writer_listener_;

    uint32_t high_mark_for_frag_;

    map changes_by_instance_;

    std::mutex mutex_;
};

inline Publisher::impl& get_implementation(Publisher& publisher) { return *publisher.impl_; }

} /* namespace  */
} /* namespace eprosima */
#endif
#endif /* __PUBLISHER_PUBLISHERIMPL_H__*/
