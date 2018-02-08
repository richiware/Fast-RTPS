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
 * @file WLP.h
 *
 */

#ifndef __RTPS_BUILTIN_LIVELINESS_WLP_H__
#define __RTPS_BUILTIN_LIVELINESS_WLP_H__

#ifndef DOXYGEN_SHOULD_SKIP_THIS_PUBLIC

#include <vector>

#include "../../common/Time_t.h"
#include "../../common/Locator.h"
#include "../../participant/RTPSParticipant.h"
#include "../../writer/RTPSWriter.h"
#include "../../reader/RTPSReader.h"
#include "../../history/WriterHistory.h"

namespace eprosima {
namespace fastrtps{

class WriterQos;


namespace rtps {

class PDPSimple;
class ParticipantProxyData;
class WLivelinessPeriodicAssertion;
class WLPListener;
class ReaderHistory;

/**
 * Class WLP that implements the Writer Liveliness Protocol described in the RTPS specification.
 * @ingroup LIVELINESS_MODULE
 */
class WLP
{
    friend class WLPListener;
    friend class WLivelinessPeriodicAssertion;

    public:

    /**
     * Constructor
     */
    WLP(PDPSimple* pdpsimple, RTPSParticipant::impl& participant);

    virtual ~WLP();
    /**
     * Initialize the WLP protocol.
     * @param p Pointer to the RTPS participant implementation.
     * @return true if the initialziacion was succesful.
     */
    bool init();
    /**
     * Create the endpoitns used in the WLP.
     * @return true if correct.
     */
    bool createEndpoints();
    /**
     * Assign the remote endpoints for a newly discovered RTPSParticipant.
     * @param pdata Pointer to the RTPSParticipantProxyData object.
     * @return True if correct.
     */
    bool assignRemoteEndpoints(const ParticipantProxyData& pdata);
    /**
     * Remove remote endpoints from the liveliness protocol.
     * @param pdata Pointer to the ParticipantProxyData to remove
     */
    void removeRemoteEndpoints(ParticipantProxyData* pdata);
    /**
     * Add a local writer to the liveliness protocol.
     * @param W Pointer to the RTPSWriter.
     * @param wqos Quality of service policies for the writer.
     * @return True if correct.
     */
    bool addLocalWriter(RTPSWriter::impl& writer, WriterQos& wqos);
    /**
     * Remove a local writer from the liveliness protocol.
     * @param W Pointer to the RTPSWriter.
     * @return True if removed.
     */
    bool removeLocalWriter(RTPSWriter::impl& writer);

    //!MInimum time of the automatic writers liveliness period.
    double m_minAutomatic_MilliSec;
    //!Minimum time of the manual by participant writers liveliness period.
    double m_minManRTPSParticipant_MilliSec;

    /**
     * Update local writer.
     * @param W Writer to update
     * @param wqos New writer QoS
     * @return True on success
     */
    bool updateLocalWriter(RTPSWriter::impl& writer, WriterQos& wqos);

    /**
     * Get the RTPS participant
     * @return RTPS participant
     */
    inline RTPSParticipant::impl& participant(){ return participant_; }

    private:
    //!Pointer to the local RTPSParticipant.
    RTPSParticipant::impl& participant_;
    //!Pointer to the builtinprotocol class.
    PDPSimple* pdpsimple_;
    //!Pointer to the builtinRTPSParticipantMEssageWriter.
    std::shared_ptr<RTPSWriter::impl> mp_builtinWriter;
    //!Pointer to the builtinRTPSParticipantMEssageReader.
    std::shared_ptr<RTPSReader::impl> mp_builtinReader;
    //!Writer History
    WriterHistory::impl* mp_builtinWriterHistory;
    //!Reader History
    ReaderHistory* mp_builtinReaderHistory;
    //!Listener object.
    WLPListener* mp_listener;
    //!Pointer to the periodic assertion timer object for the automatic liveliness writers.
    WLivelinessPeriodicAssertion* mp_livelinessAutomatic;
    //!Pointer to the periodic assertion timer object for the manual by RTPSParticipant liveliness writers.
    WLivelinessPeriodicAssertion* mp_livelinessManRTPSParticipant;
    //!List of the writers using automatic liveliness.
    std::vector<RTPSWriter::impl*> m_livAutomaticWriters;
    //!List of the writers using manual by RTPSParticipant liveliness.
    std::vector<RTPSWriter::impl*> m_livManRTPSParticipantWriters;
};

} // namespace rtps
} // namespace fastrtps
} // namespace eprosima

#endif
#endif // __RTPS_BUILTIN_LIVELINESS_WLP_H__WLP_H__
