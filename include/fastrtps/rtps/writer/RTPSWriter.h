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
 * @file RTPSWriter.h
 */

#ifndef __RTPS_WRITER_RTPSWRITER_H__
#define __RTPS_WRITER_RTPSWRITER_H__

#include "../history/CacheChangePool.h"

#include <memory>

namespace eprosima {
namespace fastrtps{
namespace rtps {

class RTPSParticipant;
class WriterAttributes;
class WriterHistory;
class WriterListener;
class RemoteReaderAttributes;
class FlowController;
class EndpointAttributes;

/**
 * Class RTPSWriter, manages the sending of data to the readers. Is always associated with a HistoryCache.
 * @ingroup WRITER_MODULE
 */
class RTPSWriter
{
    class ListenerLink;

    public:

        class impl;

        virtual ~RTPSWriter();

        /**
         * Create a new change based with the provided changeKind.
         * @param changeKind The type of change.
         * @param handle InstanceHandle to assign.
         * @return Pointer to the CacheChange or nullptr if incorrect.
         */
        template<typename T>
            CacheChange_ptr new_change(T &data, ChangeKind_t changeKind, const InstanceHandle_t& handle = c_InstanceHandle_Unknown)
            {
                return new_change([data]() -> uint32_t {return (uint32_t)T::getCdrSerializedSize(data);}, changeKind, handle);
            }


        RTPS_DllAPI CacheChange_ptr new_change(const std::function<uint32_t()>& dataCdrSerializedSize,
                ChangeKind_t changeKind, const InstanceHandle_t& handle = c_InstanceHandle_Unknown);

        /**
         * Add a matched reader.
         * @param ratt Pointer to the ReaderProxyData object added.
         * @return True if added.
         */
        RTPS_DllAPI virtual bool matched_reader_add(const RemoteReaderAttributes& ratt) = 0;

        /**
         * Remove a matched reader.
         * @param ratt Pointer to the object to remove.
         * @return True if removed.
         */
        RTPS_DllAPI virtual bool matched_reader_remove(const RemoteReaderAttributes& ratt) = 0;

        /**
         * Tells us if a specific Reader is matched against this writer
         * @param ratt Pointer to the ReaderProxyData object
         * @return True if it was matched.
         */
        RTPS_DllAPI virtual bool matched_reader_is_matched(const RemoteReaderAttributes& ratt) = 0;

        RTPS_DllAPI virtual bool wait_for_all_acked(const Duration_t& /*max_wait*/){ return true; }

        /**
         * Update the Attributes of the Writer.
         * @param att New attributes
         */
        RTPS_DllAPI virtual void updateAttributes(WriterAttributes& att) = 0;

        /**
         * Get listener
         * @return Listener
         */
        RTPS_DllAPI WriterListener* getListener();

        RTPS_DllAPI EndpointAttributes* getAttributes();

        /**
         * Get the asserted liveliness
         * @return Asserted liveliness
         */
        RTPS_DllAPI bool getLivelinessAsserted();

        /**
         * Get the asserted liveliness
         * @return asserted liveliness
         */
        RTPS_DllAPI void setLivelinessAsserted(bool l);

    private:

        std::unique_ptr<ListenerLink> listener_link_;

        RTPSWriter& operator=(const RTPSWriter&) = delete;

        friend impl& get_implementation(RTPSWriter& writer);

    protected:

        RTPSWriter(RTPSParticipant& participant, const WriterAttributes& attributes, WriterHistory& history,
                WriterListener* listener = nullptr);

        std::shared_ptr<impl> impl_;
};
}
} /* namespace rtps */
} /* namespace eprosima */

#endif /* __RTPS_WRITER_RTPSWRITER_H__ */
