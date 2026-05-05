// AStarPlanner mbf_simple_core::SimplePlanner adapter implementation.
// All algorithm work happens in texnitis_nav_core::AStarPlanner; this
// file is just glue: ROS parameters in, ROS messages out, errors mapped
// to mbf outcomes.

#include "texnitis_mbf_planners/astar_planner.hpp"

#include <pluginlib/class_list_macros.hpp>

#include <texnitis_mbf_common/error_codes.hpp>
#include <texnitis_mbf_common/pose_conversions.hpp>
#include <texnitis_mbf_common/ros_logger_bridge.hpp>

namespace texnitis::mbf_planners {

namespace ec  = ::texnitis::mbf_common::error_codes;
namespace mbc = ::texnitis::mbf_common;
namespace nc  = ::texnitis::nav_core;

namespace {

/// パラメータ宣言と取得を一括するヘルパ。`name` プレフィックス付き。
template <typename T>
T declareOrGet (const rclcpp::Node::SharedPtr &node, const std::string &full_name, const T &default_value) {
    if (!node->has_parameter (full_name)) {
        node->declare_parameter<T> (full_name, default_value);
    }
    return node->get_parameter (full_name).get_value<T> ();
}

}  // namespace

void AStarPlanner::initialize (const std::string             name,
                                const rclcpp::Node::SharedPtr &node_handle) {
    name_ = name;
    node_ = node_handle;

    // Topic / frame settings (per plugin instance).
    map_topic_     = declareOrGet<std::string> (node_handle, name + ".map_topic", std::string ("/map"));
    global_frame_  = declareOrGet<std::string> (node_handle, name + ".global_frame", std::string ("map"));

    // AStarParams from ROS parameters with the per-plugin prefix.
    nc::AStarParams params;
    params.occupied_threshold   = static_cast<int> (declareOrGet<int64_t> (
        node_handle, name + ".occupied_threshold", static_cast<int64_t> (params.occupied_threshold)));
    params.unknown_is_obstacle  = declareOrGet<bool> (
        node_handle, name + ".unknown_is_obstacle", params.unknown_is_obstacle);
    params.inflation_radius     = declareOrGet<double> (
        node_handle, name + ".inflation_radius", params.inflation_radius);
    params.max_iterations       = static_cast<int> (declareOrGet<int64_t> (
        node_handle, name + ".max_iterations", static_cast<int64_t> (params.max_iterations)));
    params.heuristic_weight     = declareOrGet<double> (
        node_handle, name + ".heuristic_weight", params.heuristic_weight);
    params.allow_diagonal       = declareOrGet<bool> (
        node_handle, name + ".allow_diagonal", params.allow_diagonal);

    core_ = std::make_unique<nc::AStarPlanner> (params);
    core_->setLogger (mbc::makeRosLoggerBridge (node_handle->get_logger ().get_child (name)));

    map_provider_ = mbc::MapProvider::get (node_handle);
    if (map_provider_) {
        map_provider_->subscribe (map_topic_);
    }

    // 計画完了ごとに `nav_msgs/Path` を publish する。topic 名は
    // `<node>/<planner_name>/plan`（例: /move_base_flex/astar/plan）。
    // transient_local QoS なので rviz2 が後から繋いでも最新計画を取れる。
    plan_pub_ = node_handle->create_publisher<nav_msgs::msg::Path> (
        name_ + "/plan", rclcpp::QoS (1).reliable ().transient_local ());

    RCLCPP_INFO (node_handle->get_logger (),
                 "AStarPlanner '%s' initialized (map_topic=%s, global_frame=%s, plan_topic=%s/plan)",
                 name_.c_str (), map_topic_.c_str (), global_frame_.c_str (), name_.c_str ());
}

uint32_t AStarPlanner::makePlan (const geometry_msgs::msg::PoseStamped               &start,
                                  const geometry_msgs::msg::PoseStamped               &goal,
                                  double                                                tolerance,
                                  std::vector<geometry_msgs::msg::PoseStamped>         &plan,
                                  double                                                &cost,
                                  std::string                                           &message) {
    (void) tolerance;  // A* operates on the inflated grid, no tolerance arg.

    cancel_requested_.store (false, std::memory_order_relaxed);
    plan.clear ();
    cost = 0.0;
    message.clear ();

    if (!core_) {
        message = "AStarPlanner: not initialized";
        return ec::kNotInitialized;
    }

    auto map_msg = map_provider_ ? map_provider_->latest (map_topic_) : nullptr;
    if (!map_msg) {
        message = "AStarPlanner: no map available on " + map_topic_;
        return ec::kNotInitialized;
    }

    const auto map_view = mbc::toGridMapView (*map_msg);
    const auto start_2d = mbc::toPose2D (start);
    const auto goal_2d  = mbc::toPose2D (goal);

    nc::Path2D path_2d;
    const auto plan_status = core_->planPath (map_view, start_2d, goal_2d, path_2d);
    if (plan_status != nc::PlanResult::Success) {
        message = std::string ("AStarPlanner: ") + nc::toString (plan_status);
        return ec::toMbfPlannerOutcome (plan_status);
    }

    rclcpp::Time stamp;
    if (auto node = node_.lock ()) {
        stamp = node->now ();
    }
    plan = mbc::toPoseStampedVector (path_2d, global_frame_, stamp);

    // Cost: rough estimate using cumulative segment length [m]. mbf
    // adopts no specific unit; consumers usually compare relatively.
    cost = 0.0;
    for (size_t i = 1; i < path_2d.poses.size (); ++i) {
        cost += nc::distanceXY (path_2d.poses[i - 1], path_2d.poses[i]);
    }

    // rviz2 / 外部監視向けに nav_msgs/Path として publish。mbf アクション
    // 結果に乗る plan とは別経路で、購読者は最新計画を topic から直接
    // 取れる（transient_local QoS）。
    if (plan_pub_) {
        nav_msgs::msg::Path path_msg;
        path_msg.header.stamp    = stamp;
        path_msg.header.frame_id = global_frame_;
        path_msg.poses           = plan;
        plan_pub_->publish (path_msg);
    }
    return ec::kSuccess;
}

bool AStarPlanner::cancel () {
    cancel_requested_.store (true, std::memory_order_relaxed);
    if (core_) {
        core_->cancel ();
    }
    return true;
}

}  // namespace texnitis::mbf_planners

PLUGINLIB_EXPORT_CLASS (texnitis::mbf_planners::AStarPlanner,
                        mbf_simple_core::SimplePlanner)
