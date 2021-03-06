/**
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef __STOUT_OS_WINDOWS_SU_HPP__
#define __STOUT_OS_WINDOWS_SU_HPP__
#include <string>

#include <stout/error.hpp>
#include <stout/nothing.hpp>
#include <stout/try.hpp>

namespace os {

inline Result<uid_t> getuid(const Option<std::string>& user = None())
{
  return WindowsError(ERROR_NOT_SUPPORTED);
}

inline Result<gid_t> getgid(const Option<std::string>& user = None())
{
  return WindowsError(ERROR_NOT_SUPPORTED);
}

inline Try<Nothing> su(const std::string& user)
{
  return WindowsError(ERROR_NOT_SUPPORTED);
}

inline Result<std::string> user(Option<uid_t> uid = None())
{
  return WindowsError(ERROR_NOT_SUPPORTED);
}

} // namespace os {

#endif // __STOUT_OS_WINDOWS_SU_HPP__
