/// @file
/// @brief texnitis_nav_core::LoggerFn と rclcpp::Logger を繋ぐ薄い橋。
///
/// アダプタ層 (`texnitis_mbf_planners`, `texnitis_mbf_controllers`) は
/// 各プラグインの `initialize()` 直後に
///
/// ```cpp
/// core_->setLogger(
///     texnitis::mbf_common::makeRosLoggerBridge(
///         node->get_logger().get_child(name)));
/// ```
///
/// として利用する。

#pragma once

#include <rclcpp/logger.hpp>
#include <rclcpp/logging.hpp>

#include <texnitis_nav_core/logger.hpp>

#include <string>

namespace texnitis::mbf_common {

/// @brief rclcpp::Logger に出力する `LoggerFn` を作る。
///
/// LogLevel は `Debug → DEBUG`, `Info → INFO`, `Warn → WARN`,
/// `Error → ERROR` の単純対応。throttle が必要なら呼び出し側で
/// 再ラップする（コア側の出力頻度が問題になった例は今のところ無い）。
[[nodiscard]] inline ::texnitis::nav_core::LoggerFn
makeRosLoggerBridge (rclcpp::Logger logger) {
    return [logger] (::texnitis::nav_core::LogLevel level, std::string_view message) {
        // string_view → std::string にコピーするのは rclcpp の
        // `LOG_STREAM` マクロが C 文字列を期待するため。短い 1 行
        // ログのみ想定なので影響は小さい。
        const std::string copy (message.data (), message.size ());
        switch (level) {
            case ::texnitis::nav_core::LogLevel::Debug:
                RCLCPP_DEBUG (logger, "%s", copy.c_str ());
                break;
            case ::texnitis::nav_core::LogLevel::Info:
                RCLCPP_INFO (logger, "%s", copy.c_str ());
                break;
            case ::texnitis::nav_core::LogLevel::Warn:
                RCLCPP_WARN (logger, "%s", copy.c_str ());
                break;
            case ::texnitis::nav_core::LogLevel::Error:
                RCLCPP_ERROR (logger, "%s", copy.c_str ());
                break;
        }
    };
}

}  // namespace texnitis::mbf_common
