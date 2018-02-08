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
 * @file EDPSimple.h
 *
 */

#ifndef __RTPS_BUILDTIN_DISCOVERY_ENDPOINT_EDPSIMPLE_H__
#define __RTPS_BUILDTIN_DISCOVERY_ENDPOINT_EDPSIMPLE_H__
#ifndef DOXYGEN_SHOULD_SKIP_THIS_PUBLIC

#include "EDP.h"
#include "../../../reader/StatefulReader.h"
#include "../../../writer/RTPSWriter.h"
#include "../../../history/WriterHistory.h"

#include <memory>

namespace eprosima {
namespace fastrtps{
namespace rtps {

class EDPSimpleListener;
class EDPSimplePUBListener;
class EDPSimpleSUBListener;
class ReaderHistory;


/**
 * Class EDPSimple, implements the Simple Endpoint Discovery Protocol defined in the RTPS specification.
 * Inherits from EDP class.
 *@ingroup DISCOVERY_MODULE
 */
class EDPSimple : public EDP
{
    class EDPStatefulReader : public StatefulReader
    {
        public:

            EDPStatefulReader(std::shared_ptr<RTPSReader::impl>& impl, EDPSimpleListener* listener);

            virtual ReaderListener* getListener() override;

            /**
             * Switch the ReaderListener kind for the Reader.
             * If the RTPSReader does not belong to the built-in protocols it switches out the old one.
             * If it belongs to the built-in protocols, it sets the new ReaderListener callbacks to be called after the 
             * built-in ReaderListener ones.
             * @param target Pointed to ReaderLister to attach
             * @return True is correctly set.
             */
            virtual bool setListener(ReaderListener* listener) override;

        private:

            EDPSimpleListener* user_listener_;
    };

    typedef std::pair<std::shared_ptr<RTPSWriter::impl>, WriterHistory::impl*> t_p_StatefulWriter;
    typedef std::pair<EDPStatefulReader*, ReaderHistory*> t_p_StatefulReader;

    public:

    /**
     * Constructor.
     * @param p Pointer to the PDPSimple
     * @param part Pointer to the RTPSParticipantImpl
     */
    EDPSimple(PDPSimple& pdpsimple, RTPSParticipant::impl& participant);

    virtual ~EDPSimple();

    //!Discovery attributes.
    BuiltinAttributes m_discovery;
    //!Pointer to the Publications Writer (only created if indicated in the DiscoveryAtributes).
    t_p_StatefulWriter mp_PubWriter;
    //!Pointer to the Subscriptions Writer (only created if indicated in the DiscoveryAtributes).
    t_p_StatefulWriter mp_SubWriter;
    //!Pointer to the Publications Reader (only created if indicated in the DiscoveryAtributes).
    t_p_StatefulReader mp_PubReader;
    //!Pointer to the Subscriptions Reader (only created if indicated in the DiscoveryAtributes).
    t_p_StatefulReader mp_SubReader;
    //!Pointer to the ReaderListener associated with PubReader
    EDPSimplePUBListener* mp_pubListen;
    //!Pointer to the ReaderListener associated with SubReader
    EDPSimpleSUBListener* mp_subListen;

    /**
     * Initialization method.
     * @param attributes Reference to the DiscoveryAttributes.
     * @return True if correct.
     */
    bool initEDP(BuiltinAttributes& attributes);
    /**
     * This method assigns the remote builtin endpoints that the remote RTPSParticipant indicates is using to our local builtin endpoints.
     * @param pdata Pointer to the RTPSParticipantProxyData object.
     */
    void assignRemoteEndpoints(const ParticipantProxyData& pdata);
    /**
     * Remove remote endpoints from the endpoint discovery protocol
     * @param pdata Pointer to the ParticipantProxyData to remove
     */
    void removeRemoteEndpoints(ParticipantProxyData* pdata);

    /**
     * Create local SEDP Endpoints based on the DiscoveryAttributes.
     * @return True if correct.
     */
    bool createSEDPEndpoints();
    /**
     * This method generates the corresponding change in the subscription writer and send it to all known remote endpoints.
     * @param rdata Pointer to the ReaderProxyData object.
     * @return true if correct.
     */
    bool processLocalReaderProxyData(ReaderProxyData* rdata);
    /**
     * This method generates the corresponding change in the publciations writer and send it to all known remote endpoints.
     * @param wdata Pointer to the WriterProxyData object.
     * @return true if correct.
     */
    bool processLocalWriterProxyData(WriterProxyData* wdata);
    /**
     * This methods generates the change disposing of the local Reader and calls the unpairing and removal methods of the base class.
     * @param R Pointer to the RTPSReader object.
     * @return True if correct.
     */
    bool removeLocalReader(RTPSReader::impl& reader);
    /**
     * This methods generates the change disposing of the local Writer and calls the unpairing and removal methods of the base class.
     * @param W Pointer to the RTPSWriter object.
     * @return True if correct.
     */
    bool removeLocalWriter(RTPSWriter::impl& writer);

};

} // namespace rtps
} // namespace fastrtps
} // namespace eprosima

#endif
#endif // __RTPS_BUILDTIN_DISCOVERY_ENDPOINT_EDPSIMPLE_H__
