#pragma once

#include <algorithm>
#include <cstddef>
#include <deque>
#include <string>
#include <vector>

namespace bl::csharp {

inline bool managedLiveCallbackCurrent(bool sessionEnabled, bool typeArmed,
                                       bool ownerRevoked) {
    return sessionEnabled && typeArmed && !ownerRevoked;
}

inline bool managedLiveSessionCanStop(bool sessionEnabled, bool primaryRevoked,
                                     bool primaryIntakeDisarmed, bool additionalArmed) {
    return sessionEnabled && primaryRevoked && primaryIntakeDisarmed &&
        !additionalArmed;
}

template <typename Job, typename Binding>
size_t eraseManagedModBoundJobs(std::deque<Job>& jobs,
                                const std::vector<Binding>& bindings,
                                const std::string& modId) {
    if (modId.empty()) return 0;
    return std::erase_if(jobs, [&](const Job& job) {
        return job.binding >= 0 &&
            static_cast<size_t>(job.binding) < bindings.size() &&
            bindings[static_cast<size_t>(job.binding)].modId == modId;
    });
}

}
