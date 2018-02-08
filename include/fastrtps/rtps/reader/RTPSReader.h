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
 * @file RTPSReader.h
 */

#ifndef __RTPS_READER_RTPSREADER_H__
#define __RTPS_READER_RTPSREADER_H__

#include "../../fastrtps_dll.h"

#include <memory>

namespace eprosima {
namespace fastrtps {
namespace rtps {

// Forward declarations
class ReaderListener;
class ReaderHistory;
struct CacheChange_t;
class WriterProxy;
struct SequenceNumber_t;
class SequenceNumberSet_t;
class FragmentedChangePitStop;
class ReaderAttributes;
class RemoteWriterAttributes;
class RTPSParticipant;
class EndpointAttributes;

/**
 * Class RTPSReader, manages the reception of data from its matched writers.
 * @ingroup READER_MODULE
 */
class RTPSReader
{
    class ListenerLink;

    public:

        class impl;

        virtual ~RTPSReader();

        /**
         * Add a matched writer represented by its attributes.
         * @param wdata Attributes of the writer to add.
         * @return True if correctly added.
         */
        RTPS_DllAPI virtual bool matched_writer_add(const RemoteWriterAttributes& wdata) = 0;

        /**
         * Remove a writer represented by its attributes from the matched writers.
         * @param wdata Attributes of the writer to remove.
         * @return True if correctly removed.
         */
        RTPS_DllAPI virtual bool matched_writer_remove(const RemoteWriterAttributes& wdata) = 0;

        /**
         * Tells us if a specific Writer is matched against this reader
         * @param wdata Pointer to the WriterProxyData object
         * @return True if it is matched.
         */
        RTPS_DllAPI virtual bool matched_writer_is_matched(const RemoteWriterAttributes& wdata) = 0;

        /**
         * Get the associated listener, secondary attached Listener in case it is of coumpound type
         * @return Pointer to the associated reader listener.
         */
        RTPS_DllAPI virtual ReaderListener* getListener();

        /**
         * Switch the ReaderListener kind for the Reader.
         * If the RTPSReader does not belong to the built-in protocols it switches out the old one.
         * If it belongs to the built-in protocols, it sets the new ReaderListener callbacks to be called after the 
         * built-in ReaderListener ones.
         * @param target Pointed to ReaderLister to attach
         * @return True is correctly set.
         */
        RTPS_DllAPI virtual bool setListener(ReaderListener* target);

        RTPS_DllAPI EndpointAttributes* getAttributes();

        /**
         * Read the next unread CacheChange_t from the history
         * @param change POinter to pointer of CacheChange_t
         * @param wp Pointer to pointer to the WriterProxy
         * @return True if read.
         */
        RTPS_DllAPI virtual bool nextUnreadCache(CacheChange_t** change, WriterProxy** wp) = 0;

        /**
         * Get the next CacheChange_t from the history to take.
         * @param change Pointer to pointer of CacheChange_t.
         * @param wp Pointer to pointer to the WriterProxy.
         * @return True if read.
         */
        RTPS_DllAPI virtual bool nextUntakenCache(CacheChange_t** change, WriterProxy** wp) = 0;

        /*!
         * @brief Returns there is a clean state with all Writers.
         * It occurs when the Reader received all samples sent by Writers. In other words,
         * its WriterProxies are up to date.
         * @return There is a clean state with all Writers.
         */
        virtual bool isInCleanState() const = 0;

    private:

        RTPSReader& operator=(const RTPSReader&) = delete;

        friend impl& get_implementation(RTPSReader& reader);

    protected:

        RTPSReader(RTPSParticipant& participant, const ReaderAttributes& att, ReaderHistory* hist,
                ReaderListener* listener = nullptr);

        RTPSReader(std::shared_ptr<impl>& impl, ReaderListener* listener = nullptr);

        std::unique_ptr<ListenerLink> listener_link_;

        std::shared_ptr<impl> impl_;
};

} //namespace rtps
} //namespace fastrtps
} //namespace eprosima

#endif // __RTPS_READER_RTPSREADER_H__
