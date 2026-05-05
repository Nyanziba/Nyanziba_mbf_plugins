// HeightAwareAStarPlanner mbf adapter implementation.

#include "texnitis_mbf_planners/height_aware_astar_planner.hpp"

#include <pluginlib/class_list_macros.hpp>

#include <texnitis_mbf_common/error_codes.hpp>
#include <texnitis_mbf_common/pose_conversions.hpp>
#include <texnitis_mbf_common/ros_logger_bridge.hpp>

namespace texnitis::mbf_planners {

namespace ec  = ::texnitis::mbf_common::error_codes;
namespace mbc = ::texnitis::mbf_common;
namespace nc  = ::texnitis::nav_core;

namespace {

template <typename T>
T declareOrGet (const rclcpp::Node::SharedPtr &node, const std::string &full_name, const T &default_value) {
    if (!node->has_parameter (full_name)) {
        node->declare_parameter<T> (full_name, default_value);
    }
    return node->get_parameter (full_name).get_value<T> ();
}

}  // namespace

void HeightAwareAStarPlanner::initialize (const std::string             name,
                                           const rclcpp::Node::SharedPtr &node_handle) {
    name_         = name;
    node_         = node_handle;
    map_topic_    = declareOrGet<std::string> (node_handle, name + ".map_topic", std::string ("/map"));
    height_topic_ = declareOrGet<std::string> (node_handle, name + ".height_topic", std::string ("/height_grid"));
    global_frame_ = declareOrGet<std::string> (node_handle, name + ".global_frame", std::string ("map"));

    nc::HeightAwareAStarParams params;
    params.astar.occupied_threshold   = static_cast<int> (declareOrGet<int64_t> (
        node_handle, name + ".occupied_threshold", static_cast<int64_t> (params.astar.occupied_threshold)));
    params.astar.unknown_is_obstacle  = declareOrGet<bool> (
        node_handle, name + ".unknown_is_obstacle", params.astar.unknown_is_obstacle);
    params.astar.inflation_radius     = declareOrGet<double> (
        node_handle, name + ".inflation_radius", params.astar.inflation_radius);
    params.astar.heuristic_weight     = declareOrGet<double> (
        node_handle, name + ".heuristic_weight", params.astar.heuristic_weight);
    params.astar.allow_diagonal       = declareOrGet<bool> (
        node_handle, name + ".allow_diagonal", params.astar.allow_diagonal);

    params.height_lethal_threshold = static_cast<int> (declareOrGet<int64_t> (
        node_handle, name + ".height_lethal_threshold",
        static_cast<int64_t> (params.height_lethal_threshold)));
    params.require_height_grid     = declareOrGet<bool> (
        node_handle, name + ".require_height_grid", params.require_height_grid);

    core_ = std::make_unique<nc::HeightAwareAStarPlanner> (params, &cached_height_);
    core_->setLogger (mbc::makeRosLoggerBridge (node_handle->get_logger ().get_child (name)));

    map_provider_    = mbc::MapProvider::get (node_handle);
    height_provider_ = mbc::HeightMapProvider::get (node_handle);
    if (map_provider_) {
        map_provider_->subscribe (map_topic_);
    }
    if (height_provider_) {
        height_provider_->subscribe (height_topic_);
    }

    plan_pub_ = node_handle->create_publisher<nav_msgs::msg::Path> (
        name_ + "/plan", rclcpp::QoS (1).reliable ().transient_local ());

    RCLCPP_INFO (node_handle->get_logger (),
                 "HeightAwareAStarPlanner '%s' initialized (map=%s height=%s plan=%s/plan)",
                 name_.c_str (), map_topic_.c_str (), height_topic_.c_str (), name_.c_str ());
}

uint32_t HeightAwareAStarPlanner::makePlan (
    const geometry_msgs::msg::PoseStamped               &start,
    const geometry_msgs::msg::PoseStamped               &goal,
    double                                                tolerance,
    std::vector<geometry_msgs::msg::PoseStamped>         &plan,
    double                                                &cost,
    std::string                                           &message) {
    (void) tolerance;
    cancel_requested_.store (false, std::memory_order_relaxed);
    plan.clear ();
    cost = 0.0;
    message.clear ();

    if (!core_) {
        message = "HeightAwareAStarPlanner: not initialized";
        return ec::kNotInitialized;
    }

    auto map_msg = map_provider_ ? map_provider_->latest (map_topic_) : nullptr;
    if (!map_msg) {
        message = "HeightAwareAStarPlanner: no map on " + map_topic_;
        return ec::kNotInitialized;
    }
    cached_height_.set (height_provider_ ? height_provider_->latest (height_topic_) : nullptr);

    const auto map_view = mbc::toGridMapView (*map_msg);
    const auto start_2d = mbc::toPose2D (start);
    const auto goal_2d  = mbc::toPose2D (goal);

    nc::Path2D path_2d;
    const auto plan_status = core_->planPath (map_view, start_2d, goal_2d, path_2d);
    if (plan_status != nc::PlanResult::Success) {
        message = std::string ("HeightAwareAStarPlanner: ") + nc::toString (plan_status);
        return ec::toMbfPlannerOutcome (plan_status);
    }

    rclcpp::Time stamp;
    if (auto node = node_.lock ()) {
        stamp = node->now ();
    }
    plan = mbc::toPoseStampedVector (path_2d, global_frame_, stamp);
    cost = 0.0;
    for (size_t i = 1; i < path_2d.poses.size (); ++i) {
        cost += nc::distanceXY (path_2d.poses[i - 1], path_2d.poses[i]);
    }

    if (plan_pub_) {
        nav_msgs::msg::Path path_msg;
        path_msg.header.stamp    = stamp;
        path_msg.header.frame_id = global_frame_;
        path_msg.poses           = plan;
        plan_pub_->publish (path_msg);
    }
    return ec::kSuccess;
}

bool HeightAwareAStarPlanner::cancel () {
    cancel_requested_.store (true, std::memory_order_relaxed);
    if (core_) {
        core_->cancel ();
    }
    return true;
}

}  // namespace texnitis::mbf_planners

PLUGINLIB_EXPORT_CLASS (texnitis::mbf_planners::HeightAwareAStarPlanner,
                        mbf_simple_core::SimplePlanner)
