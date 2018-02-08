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
 * @file Endpoint.h
 */

#ifndef __RTPS_ENDPOINT_H__
#define __RTPS_ENDPOINT_H__

#include "common/Types.h"
#include "common/Locator.h"
#include "common/Guid.h"
#include "attributes/EndpointAttributes.h"
#include "participant/RTPSParticipant.h"

#include <mutex>

namespace eprosima {
namespace fastrtps{
namespace rtps {

class ResourceEvent;

/**
 * Class Endpoint, all entities of the RTPS network derive from this class.
 * Although the RTPSParticipant is also defined as an endpoint in the RTPS specification, in this implementation
 * the RTPSParticipant class **does not** inherit from the endpoint class. Each Endpoint object owns a pointer to the
 * RTPSParticipant it belongs to.
 * @ingroup COMMON_MODULE
 */
class Endpoint
{
    protected:

    Endpoint(RTPSParticipant::impl& participant, const GUID_t& guid, const EndpointAttributes& att);

    virtual ~Endpoint();

    public:

    /**
     * Get associated GUID
     * @return Associated GUID
     */
    RTPS_DllAPI inline const GUID_t& getGuid() const { return guid_; };

    /**
     * Get associated attributes
     * @return Endpoint attributes
     */
    RTPS_DllAPI inline EndpointAttributes* getAttributes() { return &att_; }

#if HAVE_SECURITY
    bool supports_rtps_protection() { return supports_rtps_protection_; }

    void supports_rtps_protection(bool value) { supports_rtps_protection_ = value; }

    bool is_submessage_protected() { return is_submessage_protected_; }

    void is_submessage_protected(bool value) { is_submessage_protected_ = value; }

    bool is_payload_protected() { return is_payload_protected_; }

    void is_payload_protected(bool value) { is_payload_protected_ = value; }
#endif

    //TODO(Ricardo) hide
    //!Endpoint Attributes
    EndpointAttributes att_;

    //!Endpoint GUID
    const GUID_t guid_;

    protected:

    //!Pointer to the RTPSParticipant containing this endpoint.
    RTPSParticipant::impl& participant_;

    private:

    Endpoint& operator=(const Endpoint&) = delete;

#if HAVE_SECURITY
    bool supports_rtps_protection_;

    bool is_submessage_protected_;

    bool is_payload_protected_;
#endif
};


} //namespace rtps
} //namespace fastrtps
} //namespace eprosima

#endif //__RTPS_ENDPOINT_H__
