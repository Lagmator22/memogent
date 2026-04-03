// SPDX-License-Identifier: MIT
#pragma once

#include <string_view>

namespace mem::log {

enum class Level : int { Trace, Debug, Info, Warn, Error, Off };

void init(Level lvl = Level::Info);
void set_level(Level lvl);
void trace(std::string_view msg);
void debug(std::string_view msg);
void info(std::string_view msg);
void warn(std::string_view msg);
void error(std::string_view msg);

}  // namespace mem::log
