// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef __WINDOWS_ISOLATOR_HPP__
#define __WINDOWS_ISOLATOR_HPP__

#include <process/future.hpp>

#include <stout/hashmap.hpp>
#include <stout/os.hpp>

#include <stout/os/pstree.hpp>

#include "slave/flags.hpp"

#include "slave/containerizer/mesos/isolator.hpp"

#include "usage/usage.hpp"

namespace mesos {
namespace internal {
namespace slave {

// A basic MesosIsolatorProcess that keeps track of the pid but
// doesn't do any resource isolation. Subclasses must implement
// usage() for their appropriate resource(s).
class WindowsIsolatorProcess : public MesosIsolatorProcess
{
public:
  virtual process::Future<Nothing> recover(
      const std::list<mesos::slave::ContainerState>& state,
      const hashset<ContainerID>& orphans)
  {
    foreach (const mesos::slave::ContainerState& run, state) {
      // This should (almost) never occur: see comment in
      // PosixLauncher::recover().
      if (pids.contains(run.container_id())) {
        return process::Failure("Container already recovered");
      }

      pids.put(run.container_id(), run.pid());

      process::Owned<process::Promise<mesos::slave::ContainerLimitation>>
        promise(new process::Promise<mesos::slave::ContainerLimitation>());
      promises.put(run.container_id(), promise);
    }

    return Nothing();
  }

  virtual process::Future<Option<mesos::slave::ContainerLaunchInfo>> prepare(
      const ContainerID& containerId,
      const mesos::slave::ContainerConfig& containerConfig)
  {
    if (promises.contains(containerId)) {
      return process::Failure("Container " + stringify(containerId) +
                              " has already been prepared");
    }

    process::Owned<process::Promise<mesos::slave::ContainerLimitation>> promise(
        new process::Promise<mesos::slave::ContainerLimitation>());
    promises.put(containerId, promise);

    return None();
  }

  virtual process::Future<Nothing> isolate(
      const ContainerID& containerId,
      pid_t pid)
  {
    if (!promises.contains(containerId)) {
      return process::Failure("Unknown container: " + stringify(containerId));
    }

    pids.put(containerId, pid);

    return Nothing();
  }

  virtual process::Future<mesos::slave::ContainerLimitation> watch(
      const ContainerID& containerId)
  {
    if (!promises.contains(containerId)) {
      return process::Failure("Unknown container: " + stringify(containerId));
    }

    return promises[containerId]->future();
  }

  virtual process::Future<Nothing> update(
      const ContainerID& containerId,
      const Resources& resources)
  {
    if (!promises.contains(containerId)) {
      return process::Failure("Unknown container: " + stringify(containerId));
    }

    // No resources are actually isolated so nothing to do.
    return Nothing();
  }

  virtual process::Future<Nothing> cleanup(const ContainerID& containerId)
  {
    if (!promises.contains(containerId)) {
      return process::Failure("Unknown container: " + stringify(containerId));
    }

    // TODO(idownes): We should discard the container's promise here to signal
    // to anyone that holds the future from watch().
    promises.erase(containerId);

    pids.erase(containerId);

    return Nothing();
  }

protected:
  hashmap<ContainerID, pid_t> pids;
  hashmap<ContainerID,
          process::Owned<process::Promise<mesos::slave::ContainerLimitation>>>
    promises;
};

class WindowsCpuIsolatorProcess : public WindowsIsolatorProcess
{
public:
  static Try<mesos::slave::Isolator*> create(const Flags& flags)
  {
    process::Owned<MesosIsolatorProcess> process(
      new WindowsCpuIsolatorProcess());

    return new MesosIsolator(process);
  }

  virtual process::Future<ResourceStatistics> usage(
    const ContainerID& containerId)
  {
    return ResourceStatistics();
  }

protected:
  WindowsCpuIsolatorProcess() {}
};

class WindowsMemIsolatorProcess : public WindowsIsolatorProcess
{
public:
  static Try<mesos::slave::Isolator*> create(const Flags& flags)
  {
    process::Owned<MesosIsolatorProcess> process(
        new WindowsMemIsolatorProcess());

    return new MesosIsolator(process);
  }

  virtual process::Future<ResourceStatistics> usage(
      const ContainerID& containerId)
  {
    if (!pids.contains(containerId)) {
      LOG(WARNING) << "No resource usage for unknown container '"
                   << containerId << "'";
      return ResourceStatistics();
    }

/*
    // Use 'mesos-usage' but only request 'mem_' values.
    Try<ResourceStatistics> usage =
      mesos::internal::usage(pids.get(containerId).get(), true, false);
    if (usage.isError()) {
      return process::Failure(usage.error());
    }
    return usage.get();
*/
    return ResourceStatistics();
  }

private:
  WindowsMemIsolatorProcess() {}
};

} // namespace slave {
} // namespace internal {
} // namespace mesos {

#endif // __POSIX_ISOLATOR_HPP__
