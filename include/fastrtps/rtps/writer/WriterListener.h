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
 * @file WriterListener.h
 *
 */

#ifndef __RTPS_WRITER_WRITERLISTENER_H__
#define __RTPS_WRITER_WRITERLISTENER_H__

#include "../common/MatchingInfo.h"

namespace eprosima{
namespace fastrtps{
namespace rtps{

class RTPSWriter;

/**
* Class WriterListener with virtual method so the user can implement callbacks to certain events.
*  @ingroup WRITER_MODULE
*/
class RTPS_DllAPI WriterListener
{
    public:

        WriterListener(){};

        virtual ~WriterListener(){};

        /**
         * This method is called when a new Reader is matched with this Writer by hte builtin protocols
         * @param writer Pointer to the RTPSWriter.
         * @param info Matching Information.
         */
        virtual void onWriterMatched(RTPSWriter& writer, const MatchingInfo& info)
        { (void)writer; (void)info; }
};

}
}
}

#endif // __RTPS_WRITER_WRITERLISTENER_H__
