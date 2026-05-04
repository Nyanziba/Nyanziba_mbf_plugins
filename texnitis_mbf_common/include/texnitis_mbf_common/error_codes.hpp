/// @file
/// @brief texnitis_nav_core::PlanResult / ControllerResult ↔ mbf
///        outcome の対応表。エラー変換を 1 箇所に集約する。
///
/// mbf は `uint32_t outcome` を返す（`mbf_msgs/GetPathResult`,
/// `mbf_msgs/ExePathResult` 参照）。SUCCESS=0、planner 系は 50 番台、
/// controller 系は 100 番台。一覧は
/// [docs/reference/error_codes.md](../../docs/reference/error_codes.md)
/// に転記する。

#pragma once

#include <cstdint>

#include <texnitis_nav_core/result.hpp>

namespace texnitis::mbf_common::error_codes {

// -----------------------------------------------------------------------------
// Numeric outcome constants (mirrors mbf_msgs/{GetPathResult,ExePathResult}).
// We hold our own copy here to avoid pulling mbf_msgs into header consumers
// who only care about the mapping.
// -----------------------------------------------------------------------------

/// @name Planner outcomes
/// @{
inline constexpr uint32_t kSuccess          = 0u;
inline constexpr uint32_t kCancelled        = 51u;
inline constexpr uint32_t kInvalidStart     = 52u;
inline constexpr uint32_t kInvalidGoal      = 53u;
inline constexpr uint32_t kBlockedStart     = 54u;
inline constexpr uint32_t kBlockedGoal      = 55u;
inline constexpr uint32_t kNoPathFound      = 50u;
inline constexpr uint32_t kPatExceeded      = 56u;
inline constexpr uint32_t kEmptyPath        = 57u;
inline constexpr uint32_t kTfErrorPlanner   = 58u;
inline constexpr uint32_t kNotInitialized   = 59u;
inline constexpr uint32_t kInternalError    = 60u;
/// @}

/// @name Controller outcomes
/// @{
inline constexpr uint32_t kCtrlSuccess          = 0u;
inline constexpr uint32_t kCtrlNoValidCmd       = 100u;
inline constexpr uint32_t kCtrlPatExceeded      = 101u;
inline constexpr uint32_t kCtrlCollision        = 102u;
inline constexpr uint32_t kCtrlTimedOut         = 103u;
inline constexpr uint32_t kCtrlInvalidPath      = 104u;
inline constexpr uint32_t kCtrlTfError          = 106u;
inline constexpr uint32_t kCtrlCancelled        = 107u;
inline constexpr uint32_t kCtrlNotInitialized   = 108u;
inline constexpr uint32_t kCtrlOscillation      = 109u;
inline constexpr uint32_t kCtrlInternalError    = 110u;
/// @}

// -----------------------------------------------------------------------------
// Mapping
// -----------------------------------------------------------------------------

/// @brief PlanResult → mbf planner outcome.
///
/// `switch` で全ケースを書いてあるので、enum が拡張されたら
/// コンパイル時に警告が出る。
[[nodiscard]] inline constexpr uint32_t toMbfPlannerOutcome (
    ::texnitis::nav_core::PlanResult result) noexcept {
    using PR = ::texnitis::nav_core::PlanResult;
    switch (result) {
        case PR::Success:           return kSuccess;
        case PR::StartOutOfBounds:  return kInvalidStart;
        case PR::GoalOutOfBounds:   return kInvalidGoal;
        case PR::StartInCollision:  return kBlockedStart;
        case PR::GoalInCollision:   return kBlockedGoal;
        case PR::NoPathFound:       return kNoPathFound;
        case PR::InvalidMap:        return kInvalidStart;
        case PR::Cancelled:         return kCancelled;
        case PR::NotInitialized:    return kNotInitialized;
        case PR::InternalError:     return kInternalError;
    }
    return kInternalError;
}

/// @brief ControllerResult → mbf controller outcome.
[[nodiscard]] inline constexpr uint32_t toMbfControllerOutcome (
    ::texnitis::nav_core::ControllerResult result) noexcept {
    using CR = ::texnitis::nav_core::ControllerResult;
    switch (result) {
        case CR::Success:               return kCtrlSuccess;
        case CR::GoalReached:           return kCtrlSuccess;
        case CR::EmptyPath:             return kCtrlInvalidPath;
        case CR::PathTooFarFromRobot:   return kCtrlInvalidPath;
        case CR::ComputationFailed:     return kCtrlNoValidCmd;
        case CR::Cancelled:             return kCtrlCancelled;
        case CR::NotInitialized:        return kCtrlNotInitialized;
        case CR::InternalError:         return kCtrlInternalError;
    }
    return kCtrlInternalError;
}

}  // namespace texnitis::mbf_common::error_codes
