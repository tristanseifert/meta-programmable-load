#ifndef PLCOMMON_LOGGING_H
#define PLCOMMON_LOGGING_H

#include <cstddef>

#include <plog/Log.h>

namespace PlCommon {
void InitLogging(const int logLevel = 0, const bool simple = false);
}

#endif
