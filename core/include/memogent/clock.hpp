// SPDX-License-Identifier: MIT
#pragma once

#include <memogent/types.hpp>

namespace mem {

// Pure-virtual clock so tests can advance time deterministically.
class Clock {
public:
    virtual ~Clock() = default;
    virtual Timestamp now() const noexcept = 0;
};

class SystemClock final : public Clock {
public:
    Timestamp now() const noexcept override;
};

class FakeClock final : public Clock {
public:
    explicit FakeClock(Timestamp start = 0.0) : t_(start) {}
    Timestamp now() const noexcept override { return t_; }
    void advance(double seconds) noexcept { t_ += seconds; }
    void set(Timestamp t) noexcept { t_ = t; }

private:
    Timestamp t_;
};

}  // namespace mem
