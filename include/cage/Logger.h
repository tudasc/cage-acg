#ifndef INCLUDE_CAGE_LOGGER
#define INCLUDE_CAGE_LOGGER

#include "llvm/Support/raw_ostream.h"

#include <mutex>
#include <string_view>

#ifndef CAGE_LOG_LEVEL
/*
 * Usually set at compile time: -DCAGE_LOG_LEVEL=<N>, N in [0, 3] for output
 * 3 being most verbose
 */
#define CAGE_LOG_LEVEL 2
#endif

#ifndef CAGE_LOG_BASENAME
#define CAGE_LOG_BASENAME __FILE__
#endif

namespace cage::detail {
static std::mutex print_mutex;

inline void log(std::string_view msg) {
  llvm::errs() << msg;
}
}  // namespace cage::detail

// clang-format off
#define OO_LOG_LEVEL_MSG(LEVEL_NUM, LEVEL, MSG)                                                                   \
  if constexpr ((LEVEL_NUM) <= CAGE_LOG_LEVEL) {                                                                \
    std::lock_guard<std::mutex> lock{::cage::detail::print_mutex};                                                \
    std::string logging_message;                                                                                  \
    llvm::raw_string_ostream rso(logging_message);                                                                \
    rso << (LEVEL) << CAGE_LOG_BASENAME << ":" << __func__ << ":" << __LINE__ << ":" << MSG << "\n"; /* NOLINT */ \
    ::cage::detail::log(rso.str());                                                                               \
  }

#define OO_LOG_LEVEL_MSG_BARE(LEVEL_NUM, LEVEL, MSG)               \
  if constexpr ((LEVEL_NUM) <= CAGE_LOG_LEVEL) {                 \
    std::lock_guard<std::mutex> lock{::cage::detail::print_mutex}; \
    std::string logging_message;                                   \
    llvm::raw_string_ostream rso(logging_message);                 \
    rso << (LEVEL) << " " << MSG << "\n"; /* NOLINT */             \
    ::cage::detail::log(rso.str());                                \
  }
// clang-format on

#define LOG_TRACE(MSG)   OO_LOG_LEVEL_MSG_BARE(3, "[Trace]", MSG)
#define LOG_DEBUG(MSG)   OO_LOG_LEVEL_MSG(3, "[Debug]", MSG)
#define LOG_INFO(MSG)    OO_LOG_LEVEL_MSG(2, "[Info]", MSG)
#define LOG_WARNING(MSG) OO_LOG_LEVEL_MSG(1, "[Warning]", MSG)
#define LOG_ERROR(MSG)   OO_LOG_LEVEL_MSG(1, "[Error]", MSG)
#define LOG_FATAL(MSG)   OO_LOG_LEVEL_MSG(0, "[Fatal]", MSG)
#define LOG_MSG(MSG)     llvm::errs() << MSG << "\n"; /* NOLINT */

#endif /* INCLUDE_CAGE_LOGGER */
