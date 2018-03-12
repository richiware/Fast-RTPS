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
 * @file SubscriberImpl.h
 *
 */

#ifndef __SUBSCRIBER_SUBSCRIBERIMPL_H__
#define __SUBSCRIBER_SUBSCRIBERIMPL_H__
#ifndef DOXYGEN_SHOULD_SKIP_THIS_PUBLIC

#include <fastrtps/subscriber/Subscriber.h>
#include <fastrtps/rtps/common/Locator.h>
#include <fastrtps/rtps/common/Guid.h>
#include <fastrtps/attributes/SubscriberAttributes.h>
#include <fastrtps/subscriber/SubscriberHistory.h>
#include <fastrtps/rtps/reader/ReaderListener.h>
#include <fastrtps/participant/Participant.h>
#include "../rtps/reader/RTPSReaderImpl.h"

namespace eprosima {
namespace fastrtps {

class TopicDataType;

/**
 * Class SubscriberImpl, contains the actual implementation of the behaviour of the Subscriber.
 *  @ingroup FASTRTPS_MODULE
 */
class Subscriber::impl
{
    public:

        class Listener
        {
            public:

                Listener() = default;

                virtual ~Listener() = default;

                virtual void onNewDataMessage(Subscriber::impl&) {}

                virtual void onSubscriptionMatched(Subscriber::impl&, const rtps::MatchingInfo&) {}
        };

    /**
     * @param p
     * @param ptype
     * @param attr
     * @param listen
     */
    impl(Participant::impl& participant, TopicDataType* type,
            const SubscriberAttributes& attr, Listener* listen = nullptr);

    virtual ~impl();

    bool init();

    void deinit();

    bool enable();

    /**
     * Method to block the current thread until an unread message is available
     */
    void waitForUnreadMessage();


    /** @name Read or take data methods.
     * Methods to read or take data from the History.
     */

    ///@{

    bool readNextData(void* data, SampleInfo_t* info);
    bool takeNextData(void* data, SampleInfo_t* info);

    ///@}

    /**
     * Update the Attributes of the subscriber;
     * @param att Reference to a SubscriberAttributes object to update the parameters;
     * @return True if correctly updated, false if ANY of the updated parameters cannot be updated
     */
    bool updateAttributes(SubscriberAttributes& att);

    /**
     * Get associated GUID
     * @return Associated GUID
     */
    const rtps::GUID_t& getGuid();

    /**
     * Get the Attributes of the Subscriber.
     * @return Attributes of the Subscriber.
     */
    const SubscriberAttributes& getAttributes() const {return att_;}

    /**
     * Get topic data type
     * @return Topic data type
     */
    TopicDataType* getType() {return type_;}

    /*!
     * @brief Returns there is a clean state with all Publishers.
     * It occurs when the Subscriber received all samples sent by Publishers. In other words,
     * its WriterProxies are up to date.
     * @return There is a clean state with all Publishers.
     */
    bool isInCleanState() const;

    Participant::impl& participant() { return participant_; }

    private:

    //!Participant
    Participant::impl& participant_;

    //!Pointer to associated RTPSReader
    std::shared_ptr<rtps::RTPSReader::impl> reader_;

    //! Pointer to the TopicDataType object.
    TopicDataType* type_;

    //!Attributes of the Subscriber
    SubscriberAttributes att_;

    //!History
    SubscriberHistory history_;

    //!Listener
    Listener* listener_;

    class SubscriberReaderListener : public rtps::RTPSReader::impl::Listener
    {
        public:

            SubscriberReaderListener(Subscriber::impl& subscriber) :
                subscriber_(subscriber) {}

            virtual ~SubscriberReaderListener() = default;

            void onReaderMatched(rtps::RTPSReader::impl& reader, const rtps::MatchingInfo& info) override;

            void onNewCacheChangeAdded(rtps::RTPSReader::impl& reader,
                    const rtps::CacheChange_t* const change) override;

            Subscriber::impl& subscriber_;
    } m_readerListener;
};


} // namespace fastrtps
} // namespace eprosima
#endif
#endif //__SUBSCRIBER_SUBSCRIBERIMPL_H__
