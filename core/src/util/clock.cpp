// SPDX-License-Identifier: MIT
#include <memogent/clock.hpp>

#include <chrono>

namespace mem {

Timestamp SystemClock::now() const noexcept {
    using namespace std::chrono;
    return duration<double>(system_clock::now().time_since_epoch()).count();
}

}  // namespace mem
