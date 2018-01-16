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

#include "../Endpoint.h"
#include "../messages/RTPSMessageGroup.h"
#include "../attributes/WriterAttributes.h"
#include "../history/WriterHistory.h"

#include <vector>
#include <memory>
#include <functional>
#include <chrono>

namespace eprosima {
namespace fastrtps{
namespace rtps {

class WriterListener;
class FlowController;
struct CacheChange_t;

/**
 * Class RTPSWriter, manages the sending of data to the readers. Is always associated with a HistoryCache.
 * @ingroup WRITER_MODULE
 */
class RTPSWriter : public Endpoint
{
    friend class RTPSParticipantImpl;
    friend class RTPSMessageGroup;

    protected:

    RTPSWriter(RTPSParticipantImpl*, GUID_t& guid, WriterAttributes& att, WriterHistory& hist,
            WriterListener* listen = nullptr);

    virtual ~RTPSWriter();

    public:

    /**
     * Create a new change based with the provided changeKind.
     * @param changeKind The type of change.
     * @param handle InstanceHandle to assign.
     * @return Pointer to the CacheChange or nullptr if incorrect.
     */
    template<typename T>
        CacheChange_ptr new_change(T &data, ChangeKind_t changeKind, InstanceHandle_t handle = c_InstanceHandle_Unknown)
        {
            return new_change([data]() -> uint32_t {return (uint32_t)T::getCdrSerializedSize(data);}, changeKind, handle);
        }


    RTPS_DllAPI CacheChange_ptr new_change(const std::function<uint32_t()>& dataCdrSerializedSize,
            ChangeKind_t changeKind, InstanceHandle_t handle = c_InstanceHandle_Unknown);

    RTPS_DllAPI void reuse_change(CacheChange_ptr& change);

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
     * This method triggers the send operation for unsent changes.
     * @return number of messages sent
     */
    RTPS_DllAPI virtual void send_any_unsent_changes() = 0;

    /**
     * Get maximum size of the serialized type
     * @return Maximum size of the serialized type
     */
    RTPS_DllAPI uint32_t getTypeMaxSerialized() const;

    uint32_t getMaxDataSize();

    uint32_t calculateMaxDataSize(uint32_t length);

    /**
     * Get listener
     * @return Listener
     */
    RTPS_DllAPI inline WriterListener* getListener(){ return listener_; };

    /**
     * Get the asserted liveliness
     * @return Asserted liveliness
     */
    RTPS_DllAPI inline bool getLivelinessAsserted() { return m_livelinessAsserted; };

    /**
     * Get the asserted liveliness
     * @return asserted liveliness
     */
    RTPS_DllAPI inline void setLivelinessAsserted(bool l){ m_livelinessAsserted = l; };

    /**
     * Get the publication mode
     * @return publication mode
     */
    RTPS_DllAPI inline bool isAsync(){ return is_async_; };

    /*
     * Adds a flow controller that will apply to this writer exclusively.
     */
    virtual void add_flow_controller(std::unique_ptr<FlowController> controller) = 0;

    /**
     * Get RTPS participant
     * @return RTPS participant
     */
    inline RTPSParticipantImpl* getRTPSParticipant() const {return mp_RTPSParticipant;}

    RTPS_DllAPI SequenceNumber_t next_sequence_number_nts() const;

    protected:

    //!Is the data sent directly or announced by HB and THEN send to the ones who ask for it?.
    bool push_mode_;

    //!Group created to send messages more efficiently
    RTPSMessageGroup_t m_cdrmessages;

    //!INdicates if the liveliness has been asserted
    bool m_livelinessAsserted;

    //!WriterHistory
    WriterHistory::impl& history_;

    //!Listener
    WriterListener* listener_;

    //Asynchronout publication activated
    bool is_async_;

    LocatorList_t mAllShrinkedLocatorList;

    std::vector<GUID_t> mAllRemoteReaders;

    mutable std::mutex mutex_;

    void update_cached_info_nts(std::vector<GUID_t>&& allRemoteReaders,
            std::vector<LocatorList_t>& allLocatorLists);

    /**
     * Initialize the header of hte CDRMessages.
     */
    void init_header();

    private:

    /**
     * Add a change to the unsent list.
     * @param change Pointer to the change to add.
     */
    virtual bool unsent_change_added_to_history(const CacheChange_t&) = 0;

    /**
     * Indicate the writer that a change has been removed by the history due to some HistoryQos requirement.
     * @param a_change Pointer to the change that is going to be removed.
     * @return True if removed correctly.
     */
    virtual bool change_removed_by_history(const SequenceNumber_t& sequence_number,
            const InstanceHandle_t& handle) = 0;

    //! Last CacheChange Sequence Number added to the History.
    SequenceNumber_t last_cachechange_seqnum_;

    CacheChangePool cachechange_pool_;

    RTPSWriter& operator=(const RTPSWriter&) = delete;
};
}
} /* namespace rtps */
} /* namespace eprosima */

#endif /* __RTPS_WRITER_RTPSWRITER_H__ */
