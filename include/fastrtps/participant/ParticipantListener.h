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
 * @file ParticipantListener.h
 *
 */

#ifndef __PARTICIPANT_PARTICIPANTLISTENER_H__
#define __PARTICIPANT_PARTICIPANTLISTENER_H__

#include "ParticipantDiscoveryInfo.h"

namespace eprosima {
namespace fastrtps {

class Participant;

/**
 * Class ParticipantListener, overrides behaviour towards certain events.
 * @ingroup FASTRTPS_MODULE
 */
class ParticipantListener
{
    public:
        ParticipantListener(){};
        virtual ~ParticipantListener(){};

        /**
         * This method is called when a new Participant is discovered, or a previously discovered participant changes its QOS or is removed.
         * @param p Pointer to the Participant
         * @param info DiscoveryInfo.
         */
        virtual void onParticipantDiscovery(Participant& participant, const ParticipantDiscoveryInfo& info)
        { (void)participant; (void)info; }

#if HAVE_SECURITY
        virtual void onParticipantAuthentication(Participant& participant, const ParticipantAuthenticationInfo& info)
        { (void)participant; (void)info; }
#endif
};

} // namespace fastrtps
} // namespace eprosima

#endif //__PARTICIPANT_PARTICIPANTLISTENER_H__
