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
 * @file WriterProxyLiveliness.cpp
 *
 */

#include <fastrtps/rtps/reader/timedevent/WriterProxyLiveliness.h>
#include <fastrtps/rtps/resources/ResourceEvent.h>
#include <fastrtps/rtps/common/MatchingInfo.h>
#include "../StatefulReaderImpl.h"
#include <fastrtps/rtps/reader/ReaderListener.h>
#include <fastrtps/rtps/reader/WriterProxy.h>

#include "../../participant/RTPSParticipantImpl.h"

#include <fastrtps/log/Log.h>



namespace eprosima {
namespace fastrtps{
namespace rtps {


WriterProxyLiveliness::WriterProxyLiveliness(WriterProxy& writer_proxy, double interval):
TimedEvent(writer_proxy.reader_.participant().getEventResource().getIOService(),
writer_proxy.reader_.participant().getEventResource().getThread(), interval, TimedEvent::ON_SUCCESS),
writer_proxy_(writer_proxy)
{

}

WriterProxyLiveliness::~WriterProxyLiveliness()
{
    destroy();
}

void WriterProxyLiveliness::event(EventCode code, const char* msg)
{

    // Unused in release mode.
    (void)msg;

    if(code == EVENT_SUCCESS)
    {

        logInfo(RTPS_LIVELINESS,"Deleting Writer: "<<writer_proxy_.m_att.guid);
        //		if(!writer_proxy_.isAlive())
        //		{
        //logWarning(RTPS_LIVELINESS,"Liveliness failed, leaseDuration was "<< this->getIntervalMilliSec()<< " ms");
        if(writer_proxy_.reader_.matched_writer_remove(writer_proxy_.m_att,false))
        {
            if(writer_proxy_.reader_.getListener()!=nullptr)
            {
                MatchingInfo info(REMOVED_MATCHING,writer_proxy_.m_att.guid);
                //TODO(Ricardo)Uncomment
                //writer_proxy_.reader_.getListener()->onReaderMatched(&writer_proxy_.reader_,info);
            }
        }

        writer_proxy_.mp_writerProxyLiveliness = nullptr;
        delete(&writer_proxy_);
        //		}
        //		writer_proxy_.setNotAlive();
        //		this->restart_timer();
    }
    else if(code == EVENT_ABORT)
    {
        logInfo(RTPS_LIVELINESS, "WriterProxyLiveliness aborted");
    }
    else
    {
        logInfo(RTPS_LIVELINESS,"message: " <<msg);
    }
}


}
} /* namespace rtps */
} /* namespace eprosima */
