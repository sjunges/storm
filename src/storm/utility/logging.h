#pragma once

// Load streaming operator from CARL
#include <carl/io/streamingOperators.h>
namespace l3pp {
using carl::operator<<;
}

#include <l3pp.h>

#if !defined(STORM_LOG_DISABLE_DEBUG) && !defined(STORM_LOG_DISABLE_TRACE)
#define STORM_LOG_TRACE(message) L3PP_LOG_TRACE(l3pp::Logger::getRootLogger(), message)
#else
#define STORM_LOG_TRACE(message) (void)(0)
#endif

#if !defined(STORM_LOG_DISABLE_DEBUG)
#define STORM_LOG_DEBUG(message) L3PP_LOG_DEBUG(l3pp::Logger::getRootLogger(), message)
#else
#define STORM_LOG_DEBUG(message) (void)(0)
#endif

// Define STORM_LOG_WARN, STORM_LOG_ERROR and STORM_LOG_INFO to log the given message with the corresponding log levels.
#define STORM_LOG_INFO(message) L3PP_LOG_INFO(l3pp::Logger::getRootLogger(), message)
#define STORM_LOG_WARN(message) L3PP_LOG_WARN(l3pp::Logger::getRootLogger(), message)
#define STORM_LOG_ERROR(message) L3PP_LOG_ERROR(l3pp::Logger::getRootLogger(), message)

namespace storm {
namespace utility {
// Named log channels whose level can be toggled independently of the root logger's level.
constexpr const char* STATISTICS_LOG_CHANNEL = "storm.statistics";
constexpr const char* PROGRESS_LOG_CHANNEL = "storm.progress";
}  // namespace utility
}  // namespace storm

// STORM_LOG_STATISTICS and STORM_LOG_PROGRESS log at INFO level on their own channel, so their
// visibility can be enabled/disabled independently of general INFO-level verbosity.
#define STORM_LOG_STATISTICS(message) L3PP_LOG_INFO(storm::utility::STATISTICS_LOG_CHANNEL, message)
#define STORM_LOG_PROGRESS(message) L3PP_LOG_INFO(storm::utility::PROGRESS_LOG_CHANNEL, message)
