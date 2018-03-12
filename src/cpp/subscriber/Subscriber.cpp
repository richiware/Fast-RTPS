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
 * Subscriber.cpp
 *
 */

#include "SubscriberImpl.h"
#include <fastrtps/subscriber/SubscriberListener.h>
#include "../participant/ParticipantImpl.h"
#include <fastrtps/rtps/exceptions/Error.h>
#include <fastrtps/log/Log.h>

namespace eprosima {
namespace fastrtps {

    //TODO(Ricardo) Review mutex for set listener
    class Subscriber::ListenerLink : public Subscriber::impl::Listener
    {
        public:

            ListenerLink(Subscriber& subscriber, SubscriberListener* listener) :
                subscriber_(subscriber), listener_(listener)
            {
            }

            Subscriber::impl::Listener* impl_listener()
            {
                if(listener_)
                {
                    return this;
                }

                return nullptr;
            }

            SubscriberListener* listener()
            {
                return listener_;
            }

            void listener(SubscriberListener* listener)
            {
                listener_ = listener;
            }

            virtual void onNewDataMessage(Subscriber::impl& subscriber) override
            {
                assert(listener_);
                assert(subscriber_.impl_.get() == &subscriber);
                listener_->onNewDataMessage(subscriber_);
            }

            virtual void onSubscriptionMatched(Subscriber::impl& subscriber, const rtps::MatchingInfo& info) override
            {
                assert(listener_);
                assert(subscriber_.impl_.get() == &subscriber);
                listener_->onSubscriptionMatched(subscriber_, info);
            }

        private:

            Subscriber& subscriber_;

            SubscriberListener* listener_;
    };
}
}

using namespace eprosima::fastrtps;
using namespace eprosima::fastrtps::rtps;

Subscriber::Subscriber(Participant& participant, const SubscriberAttributes& att,
        SubscriberListener* listener) :
    listener_link_(new ListenerLink(*this, listener)),
    impl_(get_implementation(participant).create_subscriber(att, listener_link_->impl_listener()))
{
    if(impl_)
    {
        if(!impl_->enable())
        {
            throw Error("Error enabling publisher");
        }
    }
    else
    {
        throw Error("Error creating publisher");
    }
}

Subscriber::~Subscriber()
{
    if(impl_ && !impl_.unique())
    {
        impl_->participant().remove_subscriber(impl_);
    }
}

const GUID_t& Subscriber::getGuid()
{
    return impl_->getGuid();
}

void Subscriber::waitForUnreadMessage()
{
    return impl_->waitForUnreadMessage();
}

bool Subscriber::readNextData(void* data,SampleInfo_t* info)
{
    return impl_->readNextData(data,info);
}
bool Subscriber::takeNextData(void* data,SampleInfo_t* info)
{
    return impl_->takeNextData(data,info);
}

bool Subscriber::updateAttributes(SubscriberAttributes& att)
{
    return impl_->updateAttributes(att);
}

SubscriberAttributes Subscriber::getAttributes() const
{
    return impl_->getAttributes();
}

bool Subscriber::isInCleanState() const
{
    return impl_->isInCleanState();
}
