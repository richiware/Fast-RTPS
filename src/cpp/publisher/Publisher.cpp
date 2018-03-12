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
 * @file Publisher.cpp
 *
 */

#include "PublisherImpl.h"
#include "../participant/ParticipantImpl.h"
#include <fastrtps/publisher/PublisherListener.h>
#include <fastrtps/rtps/exceptions/Error.h>
#include <fastrtps/log/Log.h>

namespace eprosima {
namespace fastrtps {

    //TODO(Ricardo) Review mutex for set listener
    class Publisher::ListenerLink : public Publisher::impl::Listener
    {
        public:

            ListenerLink(Publisher& publisher, PublisherListener* listener) :
                publisher_(publisher), listener_(listener)
            {
            }

            Publisher::impl::Listener* impl_listener()
            {
                if(listener_)
                {
                    return this;
                }

                return nullptr;
            }

            PublisherListener* listener()
            {
                return listener_;
            }

            void listener(PublisherListener* listener)
            {
                listener_ = listener;
            }

            virtual void onPublicationMatched(Publisher::impl& publisher,
                    const rtps::MatchingInfo& info) override
            {
                assert(listener_);
                assert(publisher_.impl_.get() == &publisher);
                listener_->onPublicationMatched(publisher_, info);
            }

        private:

            Publisher& publisher_;

            PublisherListener* listener_;
    };
}
}

using namespace eprosima::fastrtps;
using namespace eprosima::fastrtps::rtps;

Publisher::Publisher(Participant& participant, const PublisherAttributes& att,
        PublisherListener* listener) :
    listener_link_(new ListenerLink(*this, listener)),
    impl_(get_implementation(participant).create_publisher(att, listener_link_->impl_listener()))
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

Publisher::~Publisher()
{
    if(impl_ && !impl_.unique())
    {
        impl_->participant().remove_publisher(impl_);
    }
}

bool Publisher::write(void* Data)
{
    logInfo(PUBLISHER,"Writing new data");
    return impl_->create_new_change(ALIVE,Data);
}

bool Publisher::write(void* Data, WriteParams &wparams)
{
    logInfo(PUBLISHER,"Writing new data with WriteParams");
    return impl_->create_new_change_with_params(ALIVE, Data, wparams);
}

bool Publisher::dispose(void* Data)
{
    logInfo(PUBLISHER,"Disposing of Data");
    return impl_->create_new_change(NOT_ALIVE_DISPOSED,Data);
}


bool Publisher::unregister(void* Data) {
    //Convert data to serialized Payload
    logInfo(PUBLISHER,"Unregistering of Data");
    return impl_->create_new_change(NOT_ALIVE_UNREGISTERED,Data);
}

bool Publisher::dispose_and_unregister(void* Data) {
    //Convert data to serialized Payload
    logInfo(PUBLISHER,"Disposing and Unregistering Data");
    return impl_->create_new_change(NOT_ALIVE_DISPOSED_UNREGISTERED,Data);
}

bool Publisher::removeAllChange(size_t* removed )
{
    logInfo(PUBLISHER,"Removing all data from history");
    return impl_->removeAllChange(removed);
}

bool Publisher::wait_for_all_acked(const Time_t& max_wait)
{
    logInfo(PUBLISHER,"Waiting for all samples acknowledged");
    return impl_->wait_for_all_acked(max_wait);
}

const GUID_t& Publisher::getGuid()
{
    return impl_->getGuid();
}

PublisherAttributes Publisher::getAttributes() const
{
    return impl_->getAttributes();
}
