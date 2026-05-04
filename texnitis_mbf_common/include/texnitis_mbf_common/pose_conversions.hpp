/// @file
/// @brief PoseStamped / OccupancyGrid / Path / Twist と
///        texnitis_nav_core の POD 型の往復変換。
///
/// アダプタ層のプラグインはここを通してのみ ROS 型⇄コア型のコピーを
/// 行う。コピーが 1 箇所に固まっているので、後でゼロコピー化や
/// ストリーミング化したくなった場合の差し替えが容易。

#pragma once

#include <geometry_msgs/msg/pose_stamped.hpp>
#include <geometry_msgs/msg/twist_stamped.hpp>
#include <nav_msgs/msg/occupancy_grid.hpp>
#include <nav_msgs/msg/path.hpp>
#include <tf2/LinearMath/Quaternion.h>

#include <texnitis_nav_core/grid_map_view.hpp>
#include <texnitis_nav_core/types.hpp>

#include <cmath>
#include <string>
#include <vector>

namespace texnitis::mbf_common {

/// @brief PoseStamped から yaw 成分を取り出して Pose2D を返す。
[[nodiscard]] inline ::texnitis::nav_core::Pose2D
toPose2D (const geometry_msgs::msg::PoseStamped &msg) {
    const auto &q = msg.pose.orientation;
    const double siny_cosp = 2.0 * (q.w * q.z + q.x * q.y);
    const double cosy_cosp = 1.0 - 2.0 * (q.y * q.y + q.z * q.z);
    return {msg.pose.position.x, msg.pose.position.y, std::atan2 (siny_cosp, cosy_cosp)};
}

/// @brief Pose2D を PoseStamped に詰める。frame_id / stamp は呼び出し
///        側が指定する。
[[nodiscard]] inline geometry_msgs::msg::PoseStamped
toPoseStamped (const ::texnitis::nav_core::Pose2D &pose,
               const std::string                  &frame_id,
               const rclcpp::Time                 &stamp) {
    geometry_msgs::msg::PoseStamped msg;
    msg.header.frame_id   = frame_id;
    msg.header.stamp      = stamp;
    msg.pose.position.x   = pose.x;
    msg.pose.position.y   = pose.y;
    msg.pose.position.z   = 0.0;
    tf2::Quaternion q;
    q.setRPY (0.0, 0.0, pose.yaw);
    msg.pose.orientation.x = q.x ();
    msg.pose.orientation.y = q.y ();
    msg.pose.orientation.z = q.z ();
    msg.pose.orientation.w = q.w ();
    return msg;
}

/// @brief OccupancyGrid → GridMapView。borrowing のみで data コピー
///        は行わない。
[[nodiscard]] inline ::texnitis::nav_core::GridMapView
toGridMapView (const nav_msgs::msg::OccupancyGrid &msg) noexcept {
    ::texnitis::nav_core::GridMapView view;
    view.data       = msg.data.data ();
    view.width      = static_cast<int> (msg.info.width);
    view.height     = static_cast<int> (msg.info.height);
    view.resolution = msg.info.resolution;
    view.origin_x   = msg.info.origin.position.x;
    view.origin_y   = msg.info.origin.position.y;
    return view;
}

/// @brief Path2D → std::vector<PoseStamped>。
[[nodiscard]] inline std::vector<geometry_msgs::msg::PoseStamped>
toPoseStampedVector (const ::texnitis::nav_core::Path2D &path,
                     const std::string                  &frame_id,
                     const rclcpp::Time                 &stamp) {
    std::vector<geometry_msgs::msg::PoseStamped> out;
    out.reserve (path.poses.size ());
    for (const auto &pose : path.poses) {
        out.push_back (toPoseStamped (pose, frame_id, stamp));
    }
    return out;
}

/// @brief std::vector<PoseStamped> → Path2D。
[[nodiscard]] inline ::texnitis::nav_core::Path2D
toPath2D (const std::vector<geometry_msgs::msg::PoseStamped> &msgs) {
    ::texnitis::nav_core::Path2D path;
    path.poses.reserve (msgs.size ());
    for (const auto &msg : msgs) {
        path.poses.push_back (toPose2D (msg));
    }
    return path;
}

/// @brief Twist2D → TwistStamped。base_link 系前提。
[[nodiscard]] inline geometry_msgs::msg::TwistStamped
toTwistStamped (const ::texnitis::nav_core::Twist2D &twist,
                const std::string                   &frame_id,
                const rclcpp::Time                  &stamp) {
    geometry_msgs::msg::TwistStamped msg;
    msg.header.frame_id   = frame_id;
    msg.header.stamp      = stamp;
    msg.twist.linear.x    = twist.vx;
    msg.twist.linear.y    = twist.vy;
    msg.twist.angular.z   = twist.wz;
    return msg;
}

}  // namespace texnitis::mbf_common
