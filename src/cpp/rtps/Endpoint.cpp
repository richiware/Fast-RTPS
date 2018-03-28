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
 * @file Endpoint.cpp
 *
 */

#include <fastrtps/rtps/Endpoint.h>
#include "participant/RTPSParticipantImpl.h"
#include "fastrtps/rtps/attributes/WriterAttributes.h"

#include <mutex>

namespace eprosima {
namespace fastrtps{
namespace rtps {

Endpoint::Endpoint(RTPSParticipant::impl& participant, const GUID_t& guid, const EndpointAttributes& att):
    att_(att),
    guid_(guid),
    participant_(participant)
#if HAVE_SECURITY
    ,supports_rtps_protection_(true),
    is_submessage_protected_(false),
    is_payload_protected_(false)
#endif
{
}

Endpoint::~Endpoint()
{
}

} /* namespace rtps */
} /* namespace eprosima */
}
