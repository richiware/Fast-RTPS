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

#ifndef __RTPS_EXCEPTIONS_ERROR_H__
#define __RTPS_EXCEPTIONS_ERROR_H__

#include "Exception.h"

namespace eprosima {
namespace fastrtps {
namespace rtps {

/**
 * @brief This abstract class is used to create exceptions.
 * @ingroup EXCEPTIONMODULE
 */
class Error : public Exception, public std::logic_error
{
    public:

        RTPS_DllAPI Error() = default;

        /**
         * @brief Default constructor.
         * @param message An error message. This message is copied.
         */
        RTPS_DllAPI Error(const std::string& message) : Exception(message.c_str(), 1), logic_error(message) {}

        /**
         * @brief Default copy constructor.
         * @param ex Error that will be copied.
         */
        RTPS_DllAPI Error(const Error &ex) = default;

        /**
         * @brief Default move constructor.
         * @param ex Error that will be moved.
         */
        RTPS_DllAPI Error(Error&& ex) = default;

        /**
         * @brief Assigment operation.
         * @param ex Error that will be copied.
         */
        RTPS_DllAPI Error& operator=(const Error& ex) = default;

        /**
         * @brief Assigment operation.
         * @param ex Error that will be moved.
         */
        RTPS_DllAPI Error& operator=(Error&& ex) = default;


        /// @brief Default destructor.
        virtual RTPS_DllAPI ~Error() throw() = default;

        /// @brief This function throws the object as exception.
        virtual RTPS_DllAPI void raise() const { throw *this; }
};

} // namespace rtps
} // namespace fastrtps
} // namespace eprosima

#endif // __RTPS_EXCEPTIONS_ERROR_H__
