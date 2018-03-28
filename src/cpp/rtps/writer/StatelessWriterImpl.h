// Copyright 2018 Proyectos y Sistemas de Mantenimiento SL (eProsima).
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
 * @file StatelessWriterImpl.h
 */


#ifndef __RTPS_WRITER_STATELESSWRITERIMPL_H__
#define __RTPS_WRITER_STATELESSWRITERIMPL_H__
#ifndef DOXYGEN_SHOULD_SKIP_THIS_PUBLIC

#include <fastrtps/rtps/writer/StatelessWriter.h>
#include "RTPSWriterImpl.h"
#include "../participant/RTPSParticipantImpl.h"

namespace eprosima {
namespace fastrtps{
namespace rtps {


/**
 * Class StatelessWriter, specialization of RTPSWriter that manages writers that don't keep state of the matched readers.
 * @ingroup WRITER_MODULE
 */
class StatelessWriter::impl : public RTPSWriter::impl
{
    public:

    impl(RTPSParticipant::impl& participant, const GUID_t& guid, const WriterAttributes& att,
            WriterHistory::impl& history, RTPSWriter::impl::Listener* listener = nullptr);

    virtual ~impl();

    bool init() { return true; }

    void deinit();

    /**
     * Add a specific change to all ReaderLocators.
     * @param p Pointer to the change.
     */
    bool unsent_change_added_to_history(const CacheChange_t& change) override;

    /**
     * Indicate the writer that a change has been removed by the history due to some HistoryQos requirement.
     * @param a_change Pointer to the change that is going to be removed.
     * @return True if removed correctly.
     */
    bool change_removed_by_history(const SequenceNumber_t& sequence_number, const InstanceHandle_t& handle) override;
    /**
     * Add a matched reader.
     * @param ratt Attributes of the reader to add.
     * @return True if added.
     */
    bool matched_reader_add(const RemoteReaderAttributes& ratt);
    /**
     * Remove a matched reader.
     * @param ratt Attributes of the reader to remove.
     * @return True if removed.
     */
    bool matched_reader_remove(const RemoteReaderAttributes& ratt);
    /**
     * Tells us if a specific Reader is matched against this writer
     * @param ratt Attributes of the reader to check.
     * @return True if it was matched.
     */
    bool matched_reader_is_matched(const RemoteReaderAttributes& ratt);
    /**
     * Method to indicate that there are changes not sent in some of all ReaderProxy.
     */
    void send_any_unsent_changes();

    /**
     * Update the Attributes of the Writer.
     * @param att New attributes
     */
    void updateAttributes(WriterAttributes& att){
        (void)att;
        //FOR NOW THERE IS NOTHING TO UPDATE.
    };

    bool add_locator(Locator_t& loc);

    void update_unsent_changes(ReaderLocator& reader_locator,
            const SequenceNumber_t& seqNum, const FragmentNumber_t fragNum);

    //!Reset the unsent changes.
    void resent_changes();

    /**
     * Get the number of matched readers
     * @return Number of matched readers
     */
    size_t getMatchedReadersSize() const override { return m_matched_readers.size(); }

    void add_flow_controller(std::unique_ptr<FlowController> controller);

    private:

    void deinit_();

    impl& operator=(const impl&) = delete;

    std::vector<GUID_t> get_builtin_guid();

    void update_locators_nts_(const GUID_t& optionalGuid);

    std::vector<ReaderLocator> reader_locators, fixed_locators;
    std::vector<RemoteReaderAttributes> m_matched_readers;
    std::vector<std::unique_ptr<FlowController> > m_controllers;
};

} //namespace rtps
} //namespace fastrtps
} //namespace eprosima

#endif
#endif //__RTPS_WRITER_STATELESSWRITERIMPL_H__