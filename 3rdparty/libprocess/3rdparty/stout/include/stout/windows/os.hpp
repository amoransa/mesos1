// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef __STOUT_WINDOWS_OS_HPP__
#define __STOUT_WINDOWS_OS_HPP__

#include <direct.h>
#include <io.h>

#include <sys/utime.h>

#include <list>
#include <map>
#include <set>
#include <string>

#include <stout/duration.hpp>
#include <stout/none.hpp>
#include <stout/nothing.hpp>
#include <stout/option.hpp>
#include <stout/path.hpp>
#include <stout/try.hpp>
#include <stout/windows.hpp>

#include <stout/os/config.hpp>
#include <stout/os/os.hpp>
#include <stout/os/read.hpp>

#include <stout/os/raw/environment.hpp>


#include <TlHelp32.h>
#include <Psapi.h>

#define WNOHANG 0
#define hstrerror() ("")
#define SIGPIPE 100

namespace os {

inline Try<std::list<Process>> processes()
{
  return std::list<Process>();
}

inline Option<Process> process(
    pid_t pid,
    const std::list<Process>& processes)
{
  foreach(const Process& process, processes) {
    if (process.pid == pid) {
      return process;
    }
  }
  return None();
}

inline std::set<pid_t> children(
    pid_t pid,
    const std::list<Process>& processes,
    bool recursive = true)
{
  // Perform a breadth first search for descendants.
  std::set<pid_t> descendants;
  std::queue<pid_t> parents;
  parents.push(pid);
  do {
    pid_t parent = parents.front();
    parents.pop();

    // Search for children of parent.
    foreach(const Process& process, processes) {
      if (process.parent == parent) {
      // Have we seen this child yet?
        if (descendants.insert(process.pid).second) {
          parents.push(process.pid);
        }
      }
    }
  } while (recursive && !parents.empty());

  return descendants;
}

inline Try<std::set<pid_t> > children(pid_t pid, bool recursive = true)
{
  const Try<std::list<Process>> processes = os::processes();

  if (processes.isError()) {
    return Error(processes.error());
  }

  return children(pid, processes.get(), recursive);
}

inline int pagesize()
{
  SYSTEM_INFO si = {0};
  GetSystemInfo(&si);
  return si.dwPageSize;
};

inline long cpu()
{
  return 4;
};

// Sets the value associated with the specified key in the set of
// environment variables.
inline void setenv(const std::string& key,
                   const std::string& value,
                   bool overwrite = true)
{
  // Do not set the variable if already set and `overwrite` was not specified.
  if (!overwrite) {
    const DWORD bytes = ::GetEnvironmentVariable(key.c_str(), NULL, 0);
    const DWORD result = ::GetLastError();

    // Per MSDN[1], `GetEnvironmentVariable` returns 0 on error and sets the
    // error code to `ERROR_ENVVAR_NOT_FOUND` if the variable was not found.
    //
    // [1] https://msdn.microsoft.com/en-us/library/windows/desktop/ms683188(v=vs.85).aspx
    if (bytes != 0 || result != ERROR_ENVVAR_NOT_FOUND) {
      return;
    }
  }

  // `SetEnvironmentVariable` returns an error code, but we can't act on it.
  ::SetEnvironmentVariable(key.c_str(), value.c_str());
}


// Unsets the value associated with the specified key in the set of
// environment variables.
inline void unsetenv(const std::string& key)
{
  ::SetEnvironmentVariable(key.c_str(), NULL);
}

/*
// Executes a command by calling "/bin/sh -c <command>", and returns
// after the command has been completed. Returns 0 if succeeds, and
// return -1 on error (e.g., fork/exec/waitpid failed). This function
// is async signal safe. We return int instead of returning a Try
// because Try involves 'new', which is not async signal safe.
inline int system(const std::string& command)
{
  UNIMPLEMENTED;
}


// This function is a portable version of execvpe ('p' means searching
// executable from PATH and 'e' means setting environments). We add
// this function because it is not available on all systems.
//
// NOTE: This function is not thread safe. It is supposed to be used
// only after fork (when there is only one thread). This function is
// async signal safe.
inline int execvpe(const char* file, char** argv, char** envp)
{
  UNIMPLEMENTED;
}


inline Try<Nothing> chown(
    uid_t uid,
    gid_t gid,
    const std::string& path,
    bool recursive)
{
  UNIMPLEMENTED;
}


inline Try<Nothing> chmod(const std::string& path, int mode)
{
  UNIMPLEMENTED;
}


inline Try<Nothing> chroot(const std::string& directory)
{
  UNIMPLEMENTED;
}


inline Try<Nothing> mknod(
    const std::string& path,
    mode_t mode,
    dev_t dev)
{
  UNIMPLEMENTED;
}


inline Result<uid_t> getuid(const Option<std::string>& user = None())
{
  UNIMPLEMENTED;
}


inline Result<gid_t> getgid(const Option<std::string>& user = None())
{
  UNIMPLEMENTED;
}


inline Try<Nothing> su(const std::string& user)
{
  UNIMPLEMENTED;
}


inline Result<std::string> user(Option<uid_t> uid = None())
{
  UNIMPLEMENTED;
}
// Returns the list of files that match the given (shell) pattern.
inline Try<std::list<std::string>> glob(const std::string& pattern)
{
UNIMPLEMENTED;
}
*/

// Suspends execution for the given duration.
inline Try<Nothing> sleep(const Duration& duration)
{
  DWORD milliseconds = static_cast<DWORD>(duration.ms());
  ::Sleep(milliseconds);

  return Nothing();
}


// Returns the total number of cpus (cores).
inline Try<long> cpus()
{
  SYSTEM_INFO sysInfo;
  ::GetSystemInfo(&sysInfo);
  return static_cast<long>(sysInfo.dwNumberOfProcessors);
}

// Returns load struct with average system loads for the last
// 1, 5 and 15 minutes respectively.
// Load values should be interpreted as usual average loads from
// uptime(1).
inline Try<Load> loadavg()
{
  return Load();
}


// Returns the total size of main and free memory.
inline Try<Memory> memory()
{
  Memory memory;

  MEMORYSTATUSEX memory_status;
  memory_status.dwLength = sizeof(MEMORYSTATUSEX);
  if (!::GlobalMemoryStatusEx(&memory_status)) {
    return WindowsError("memory(): Could not call GlobalMemoryStatusEx");
  }

  memory.total = Bytes(memory_status.ullTotalPhys);
  memory.free = Bytes(memory_status.ullAvailPhys);
  memory.totalSwap = Bytes(memory_status.ullTotalPageFile);
  memory.freeSwap = Bytes(memory_status.ullAvailPageFile);

  return memory;
}


// Return the system information.
inline Try<UTSInfo> uname()
{
  UTSInfo info;

  OSVERSIONINFOEX os_version;
  os_version.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
  if (!::GetVersionEx((LPOSVERSIONINFO)&os_version)) {
    return WindowsError("os::uname(): Failed to call GetVersionEx");
  }

  switch (os_version.wProductType) {
  case VER_NT_DOMAIN_CONTROLLER:
  case VER_NT_SERVER:
    info.sysname = "Windows Server";
    break;
  default:
    info.sysname = "Windows";
  }

  info.release = std::to_string(os_version.dwMajorVersion) + "." +
    std::to_string(os_version.dwMinorVersion);
  info.version = std::to_string(os_version.dwBuildNumber);
  if (os_version.szCSDVersion[0] != '\0') {
    info.version.append(" ");
    info.version.append(os_version.szCSDVersion);
  }

  // Get DNS name of the local computer. First, find the size of the output
  // buffer.
  DWORD size = 0;
  if (!::GetComputerNameEx(ComputerNameDnsHostname, NULL, &size) &&
    ::GetLastError() != ERROR_MORE_DATA) {
    return WindowsError("os::uname(): Failed to call GetComputerNameEx");
  }

  std::shared_ptr<char> computer_name(
    (char *)malloc((size + 1) * sizeof(char)));

  if (!::GetComputerNameEx(ComputerNameDnsHostname, computer_name.get(),
    &size)) {
    return WindowsError("os::uname(): Failed to call GetComputerNameEx");
  }

  info.nodename = computer_name.get();

  // Get OS architecture
  SYSTEM_INFO system_info;
  ::GetNativeSystemInfo(&system_info);
  switch (system_info.wProcessorArchitecture) {
  case PROCESSOR_ARCHITECTURE_AMD64:
    info.machine = "AMD64";
    break;
  case PROCESSOR_ARCHITECTURE_ARM:
    info.machine = "ARM";
    break;
  case PROCESSOR_ARCHITECTURE_IA64:
    info.machine = "IA64";
    break;
  case PROCESSOR_ARCHITECTURE_INTEL:
    info.machine = "x86";
    break;
  default:
    info.machine = "Unknown";
  }

  return info;
}


inline size_t recv(int sockfd, void *buf, size_t len, int flags) {
  return ::recv(sockfd, (char*)buf, len, flags);
}

inline int setsockopt(int socket, int level, int option_name,
       const void *option_value, socklen_t option_len) {
  return ::setsockopt(socket, level, option_name, (const char*)option_value, option_len);
}

inline int getsockopt(int socket, int level, int option_name,
  void* option_value, socklen_t* option_len) {
  return ::getsockopt(socket, level, option_name, (char*)option_value, option_len);
}

// Looks in the environment variables for the specified key and
// returns a string representation of its value. If no environment
// variable matching key is found, None() is returned.
inline Option<std::string> getenv(const std::string& key)
{
  char* value = ::getenv(key.c_str());

  if (value == NULL) {
    return None();
  }

  return std::string(value);
}

#ifdef _WINDOWS_

inline struct tm* gmtime_r(const time_t *timep, struct tm *result)
{
  // gmtime_s returns 0 if successful.
  if (gmtime_s(result, timep) == 0)
  {
    return result;
  }

  return NULL;
}

#else // ​_WINDOWS_

inline auto gmtime_r(const time_t *timep, struct tm *result) {
  return ::gmtime_r(timep, result);
}

#endif // ​_WINDOWS_

inline pid_t waitpid(pid_t pid, int *status, int options)
{
    return 0;
}

namespace libraries {

  // Returns the full library name by adding prefix and extension to
  // library name.
  inline std::string expandName(const std::string& libraryName)
  {
    const char* prefix = "lib";
    const char* extension =
#ifdef __APPLE__
      ".dylib";
#else
      ".so";
#endif

    return prefix + libraryName + extension;
  }


  // Returns the current value of LD_LIBRARY_PATH environment variable.
  inline std::string paths()
  {
    const char* environmentVariable =
#ifdef __APPLE__
      "DYLD_LIBRARY_PATH";
#else
      "LD_LIBRARY_PATH";
#endif
    const Option<std::string> path = getenv(environmentVariable);
    return path.isSome() ? path.get() : std::string();
  }


  // Updates the value of LD_LIBRARY_PATH environment variable.
  inline void setPaths(const std::string& newPaths)
  {
    const char* environmentVariable =
#ifdef __APPLE__
      "DYLD_LIBRARY_PATH";
#else
      "LD_LIBRARY_PATH";
#endif
    os::setenv(environmentVariable, newPaths);
  }


  // Append newPath to the current value of LD_LIBRARY_PATH environment
  // variable.
  inline void appendPaths(const std::string& newPaths)
  {
    if (paths().empty()) {
      setPaths(newPaths);
    }
    else {
      setPaths(paths() + ":" + newPaths);
    }
  }

} // namespace libraries {

inline Try<bool> access(const std::string& fileName, int how)
{
  if (::_access(fileName.c_str(), how) != 0) {
    return ErrnoError("access: Could not access path '" + fileName + "'");
  }

  return true;
}

inline Result<bool> FindProcess(
  pid_t pid,
  bool& exists,
  PPROCESSENTRY32 process_entry_ptr)
{
  // Initialize output paramter 'exists'.
  exists = false;

  if (NULL == process_entry_ptr) {
    return WindowsError("os::FindProcess(): \
      'process_entry_pointer' input parameter \
      cannot be null");
  }

  // Get a snapshot of the proceses in the system.
  HANDLE snapshot_handle = CreateToolhelp32Snapshot(
    TH32CS_SNAPPROCESS,
    pid);
  if (snapshot_handle == INVALID_HANDLE_VALUE ||
    snapshot_handle == NULL) {
    return WindowsError("os::FindProcess(): \
      Failed to call CreateToolhelp32Snapshot",
      GetLastError());
  }

  std::shared_ptr<void> safe_snapshot_handle
    (snapshot_handle,
      ::CloseHandle);

  // Initialize process entry.
  ZeroMemory(process_entry_ptr, sizeof(PROCESSENTRY32));
  process_entry_ptr->dwSize = sizeof(PROCESSENTRY32);

  // Point to the first process and start loop to
  // find process.
  DWORD last_error = ERROR_SUCCESS;
  bool bcontinue = Process32First(
    safe_snapshot_handle.get(),
    process_entry_ptr);
  if (!bcontinue) {
    // No processes returned. Most likely an error but
    // will handle all paths.
    last_error = GetLastError();
    if (last_error != ERROR_NO_MORE_FILES &&
      last_error != ERROR_SUCCESS) {
      return WindowsError("os::FindProcess(): \
        Failed to call Process32Next", last_error);
    }

    return true;
  }

  while (bcontinue) {
    if (process_entry_ptr->th32ProcessID == pid) {
      exists = true;
      break;
    }

    bcontinue = Process32Next(safe_snapshot_handle.get(), process_entry_ptr);
    if (!bcontinue) {
      last_error = GetLastError();
      if (last_error != ERROR_NO_MORE_FILES &&
        last_error != ERROR_SUCCESS) {
        return WindowsError("os::FindProcess(): \
          Failed to call Process32Next", last_error);
      }
    }
  }

  return true;
}

inline Result<bool> FindProcess(
  pid_t pid,
  bool& exists)
{
  PROCESSENTRY32 process_entry;
  return FindProcess(pid, exists, &process_entry);
}

inline Result<Process> process(pid_t pid)
{
  pid_t process_id = 0;
  pid_t parent_process_id = 0;
  pid_t session_id = 0;
  std::string executable_filename = "";
  size_t wss = 0;
  double user_time = 0;
  double system_time = 0;

  // Find process with pid.
  PROCESSENTRY32 process_entry;
  bool process_exists = false;
  Result<bool> findprocess_result = FindProcess(
    pid,
    process_exists,
    &process_entry);

  if (findprocess_result.isError()) {
    return WindowsError(findprocess_result.error());
  }

  // If process does not exist simply return
  // none. No need to return error here.
  // See linux.hpp implementation logic.
  if (!process_exists) {
    return None();
  }

  // Process exists. Open process and get stats.
  // Get process id and parent process id and filename.
  process_id = process_entry.th32ProcessID;
  parent_process_id = process_entry.th32ParentProcessID;
  executable_filename = process_entry.szExeFile;

  HANDLE process_handle = OpenProcess(
    THREAD_ALL_ACCESS,
    false,
    process_id);
  if (process_handle == INVALID_HANDLE_VALUE ||
    process_handle == NULL) {
    return WindowsError("os::process(): Failed to call OpenProcess",
      GetLastError());
  }

  std::shared_ptr<void> safe_process_handle(process_handle, ::CloseHandle);

  // Get Windows Working set size (Resident set size in linux).
  PROCESS_MEMORY_COUNTERS proc_mem_counters;
  bool result = GetProcessMemoryInfo(
    safe_process_handle.get(),
    &proc_mem_counters,
    sizeof(proc_mem_counters));
  if (!result) {
    return WindowsError("os::process(): Failed to call GetProcessMemoryInfo",
      GetLastError());
  }

  wss = proc_mem_counters.WorkingSetSize;

  // Get session Id.
  result = ProcessIdToSessionId(process_id, &session_id);
  if (!result) {
    return WindowsError("os::process(): Failed to call ProcessIdToSessionId",
      GetLastError());
  }

  // Get Process CPU time.
  FILETIME create_filetime, exit_filetime, kernel_filetime, user_filetime;
  result = GetProcessTimes(
    safe_process_handle.get(),
    &create_filetime,
    &exit_filetime,
    &kernel_filetime,
    &user_filetime);
  if (!result) {
    return WindowsError("os::process(): Failed to call GetProcessTimes",
      GetLastError());
  }

  LARGE_INTEGER lKernelTime, lUserTime; // in 100 nanoseconds.
  lKernelTime.HighPart = kernel_filetime.dwHighDateTime;
  lKernelTime.LowPart = kernel_filetime.dwLowDateTime;
  lUserTime.HighPart = user_filetime.dwHighDateTime;
  lUserTime.LowPart = user_filetime.dwLowDateTime;

  system_time = lKernelTime.QuadPart / 10000000;
  user_time = lUserTime.QuadPart / 10000000;

  Try<Duration> utime = Duration::create(user_time);
  Try<Duration> stime = Duration::create(system_time);

  return Process(
    process_id,        // process id.
    parent_process_id, // parent process id.
    0,                 // group id.
    session_id,        // session id.
    Bytes(wss),        // wss.
    utime.isSome() ? utime.get() : Option<Duration>::none(),
    stime.isSome() ? stime.get() : Option<Duration>::none(),
    executable_filename,
    false);            // is not zombie process.
}

} // namespace os {


#endif // __STOUT_WINDOWS_OS_HPP__
