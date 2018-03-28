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
 * RTPSReader.cpp
 *
*/

#include "RTPSReaderImpl.h"
#include <fastrtps/rtps/history/ReaderHistory.h>
#include <fastrtps/log/Log.h>
#include "FragmentedChangePitStop.h"
#include "../participant/RTPSParticipantImpl.h"
#include <fastrtps/rtps/reader/ReaderListener.h>
#include <fastrtps/rtps/exceptions/Error.h>

#include <typeinfo>

namespace eprosima {
namespace fastrtps {
namespace rtps {

    //TODO(Ricardo) Review mutex for set listener
    class RTPSReader::ListenerLink : public RTPSReader::impl::Listener
    {
        public:

            ListenerLink(RTPSReader& reader, ReaderListener* listener) :
                reader_(reader), listener_(listener)
            {
            }

            RTPSReader::impl::Listener* rtps_listener()
            {
                if(listener_)
                {
                    return this;
                }

                return nullptr;
            }

            ReaderListener* listener()
            {
                return listener_;
            }

            void listener(ReaderListener* listener)
            {
                listener_ = listener;
            }

            virtual void onReaderMatched(RTPSReader::impl& reader, const MatchingInfo& info) override
            {
                assert(listener_);
                assert(reader_.impl_.get() == &reader);
                listener_->onReaderMatched(reader_, info);
            }

            /**
             * This method is called when a new CacheChange_t is added to the ReaderHistory.
             * @param reader Pointer to the reader.
             * @param change Pointer to the CacheChange_t. This is a const pointer to const data
             * to indicate that the user should not dispose of this data himself.
             * To remove the data call the remove_change method of the ReaderHistory.
             * reader->getHistory()->remove_change((CacheChange_t*)change).
             */
            virtual void onNewCacheChangeAdded(RTPSReader::impl& reader, const CacheChange_t* const change) override
            {
                assert(listener_);
                assert(reader_.impl_.get() == &reader);
                listener_->onNewCacheChangeAdded(reader_, change);
            }

        private:

            RTPSReader& reader_;

            ReaderListener* listener_;
    };

}
}
}

using namespace eprosima::fastrtps::rtps;

RTPSReader::RTPSReader(RTPSParticipant& participant, const ReaderAttributes& attributes,
        ReaderHistory* history, ReaderListener* listener) : listener_link_(new ListenerLink(*this, listener)),
    impl_(get_implementation(participant).create_reader(attributes, history, listener_link_->rtps_listener()))
{
    if(!impl_)
    {
        throw Error("Error creating writer");
    }

    //TODO(Ricardo) Add to RTPSParticipant.
}

RTPSReader::RTPSReader(std::shared_ptr<RTPSReader::impl>& impl, ReaderListener* listener) :
    listener_link_(new ListenerLink(*this, listener)), impl_(impl)
{
    impl_->setListener(listener_link_->rtps_listener());
}

RTPSReader::~RTPSReader()
{
    if(impl_ && !impl_.unique())
    {
        impl_->participant().remove_reader(impl_);
    }
}

RTPSReader::impl::impl(RTPSParticipant::impl& participant, const GUID_t& guid,
        const ReaderAttributes& att, ReaderHistory* hist, RTPSReader::impl::Listener* listener) :
    Endpoint(participant, guid, att.endpoint),
    mp_history(hist),
    mp_listener(listener),
    m_acceptMessagesToUnknownReaders(true),
    m_acceptMessagesFromUnkownWriters(true),
    m_expectsInlineQos(att.expectsInlineQos),
    fragmentedChangePitStop_(nullptr),
    mp_mutex(new std::recursive_mutex())
    {
        mp_history->reader_ = this;
        mp_history->mp_mutex = mp_mutex;
        fragmentedChangePitStop_ = new FragmentedChangePitStop(*this);
        logInfo(RTPS_READER,"RTPSReader created correctly");
    }

RTPSReader::impl::~impl()
{
    deinit_();
}

void RTPSReader::impl::deinit_()
{
    if(fragmentedChangePitStop_ != nullptr)
    {
        delete fragmentedChangePitStop_;
        fragmentedChangePitStop_ = nullptr;
    }

    if(mp_mutex != nullptr)
    {
        delete(mp_mutex);
        mp_mutex = nullptr;
    }

    mp_history->reader_ = nullptr;
    mp_history->mp_mutex = nullptr;
}

bool RTPSReader::impl::acceptMsgDirectedTo(EntityId_t& entityId)
{
    if(entityId == guid_.entityId)
        return true;
    if(m_acceptMessagesToUnknownReaders && entityId == c_EntityId_Unknown)
        return true;
    else
        return false;
}

CacheChange_ptr RTPSReader::impl::reserveCache()
{
    return mp_history->reserve_Cache();
}

ReaderListener* RTPSReader::getListener()
{
    return listener_link_->listener();
}

//TODO(Ricardo) Review mutex
bool RTPSReader::setListener(ReaderListener *listener)
{
    listener_link_->listener(listener);
    impl_->setListener(listener_link_->rtps_listener());
    return true;
}

EndpointAttributes* RTPSReader::getAttributes()
{
    return &impl_->att_;
}

//TODO(Ricardo) Is this getter necessary?
RTPSReader::impl::Listener* RTPSReader::impl::getListener()
{
    return mp_listener;
}

//TODO(Ricardo) Review
bool RTPSReader::impl::setListener(RTPSReader::impl::Listener *listener)
{
    mp_listener = listener;
    return true;
}

CacheChange_t* RTPSReader::impl::findCacheInFragmentedCachePitStop(const SequenceNumber_t& sequence_number,
        const GUID_t& writer_guid)
{
    return fragmentedChangePitStop_->find(sequence_number, writer_guid);
}
