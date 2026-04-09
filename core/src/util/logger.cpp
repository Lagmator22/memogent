// SPDX-License-Identifier: MIT
#include <memogent/logger.hpp>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <memory>
#include <mutex>

namespace mem::log {

namespace {
std::shared_ptr<spdlog::logger>& inst() {
    static std::shared_ptr<spdlog::logger> l;
    static std::once_flag flag;
    std::call_once(flag, [&] {
        l = spdlog::stdout_color_mt("memogent");
        l->set_pattern("%^[%T.%e] %n [%l]%$ %v");
        l->set_level(spdlog::level::info);
    });
    return l;
}

spdlog::level::level_enum to_spdlog(Level lvl) {
    switch (lvl) {
        case Level::Trace: return spdlog::level::trace;
        case Level::Debug: return spdlog::level::debug;
        case Level::Info:  return spdlog::level::info;
        case Level::Warn:  return spdlog::level::warn;
        case Level::Error: return spdlog::level::err;
        case Level::Off:   return spdlog::level::off;
    }
    return spdlog::level::info;
}
}  // namespace

void init(Level lvl) { inst()->set_level(to_spdlog(lvl)); }
void set_level(Level lvl) { inst()->set_level(to_spdlog(lvl)); }
void trace(std::string_view m) { inst()->trace("{}", m); }
void debug(std::string_view m) { inst()->debug("{}", m); }
void info(std::string_view m) { inst()->info("{}", m); }
void warn(std::string_view m) { inst()->warn("{}", m); }
void error(std::string_view m) { inst()->error("{}", m); }

}  // namespace mem::log
