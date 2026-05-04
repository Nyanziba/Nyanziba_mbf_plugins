// LookaheadController mbf adapter implementation. Delegates everything
// to texnitis_nav_core::LookaheadController. The adapter only owns ROS
// type marshalling, parameter loading, the cached current pose, and
// the cancel flag.

#include "texnitis_mbf_controllers/lookahead_controller.hpp"

#include <pluginlib/class_list_macros.hpp>

#include <texnitis_mbf_common/error_codes.hpp>
#include <texnitis_mbf_common/pose_conversions.hpp>
#include <texnitis_mbf_common/ros_logger_bridge.hpp>

namespace texnitis::mbf_controllers {

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

void LookaheadController::initialize (const std::string                  name,
                                       const std::shared_ptr<::TF>       &tf,
                                       const rclcpp::Node::SharedPtr     &node_handle) {
    name_ = name;
    node_ = node_handle;
    tf_   = tf;

    base_frame_ = declareOrGet<std::string> (
        node_handle, name + ".base_frame", std::string ("base_link"));

    nc::LookaheadParams params;
    params.kp_xy                  = declareOrGet<double> (node_handle, name + ".kp_xy", params.kp_xy);
    params.kp_yaw                 = declareOrGet<double> (node_handle, name + ".kp_yaw", params.kp_yaw);
    params.max_speed_xy           = declareOrGet<double> (node_handle, name + ".max_speed_xy", params.max_speed_xy);
    params.max_speed_yaw          = declareOrGet<double> (node_handle, name + ".max_speed_yaw", params.max_speed_yaw);
    params.lookahead_dist         = declareOrGet<double> (node_handle, name + ".lookahead_dist", params.lookahead_dist);
    params.linear_threshold_for_wz = declareOrGet<double> (
        node_handle, name + ".linear_threshold_for_wz", params.linear_threshold_for_wz);
    params.max_wz_when_moving     = declareOrGet<double> (
        node_handle, name + ".max_wz_when_moving", params.max_wz_when_moving);
    params.use_diff_drive         = declareOrGet<bool> (
        node_handle, name + ".use_diff_drive", params.use_diff_drive);

    params.goal_checker.xy_tolerance  = declareOrGet<double> (
        node_handle, name + ".goal_xy_tolerance", params.goal_checker.xy_tolerance);
    params.goal_checker.yaw_tolerance = declareOrGet<double> (
        node_handle, name + ".goal_yaw_tolerance", params.goal_checker.yaw_tolerance);
    params.goal_checker.stateful      = declareOrGet<bool> (
        node_handle, name + ".goal_stateful", params.goal_checker.stateful);

    core_ = std::make_unique<nc::LookaheadController> (params);
    core_->setLogger (mbc::makeRosLoggerBridge (node_handle->get_logger ().get_child (name)));

    RCLCPP_INFO (node_handle->get_logger (),
                 "LookaheadController '%s' initialized (diff_drive=%s)",
                 name_.c_str (), params.use_diff_drive ? "true" : "false");
}

uint32_t LookaheadController::computeVelocityCommands (
    const geometry_msgs::msg::PoseStamped  &pose,
    const geometry_msgs::msg::TwistStamped &velocity,
    geometry_msgs::msg::TwistStamped       &cmd_vel,
    std::string                            &message) {
    (void) velocity;
    cmd_vel = geometry_msgs::msg::TwistStamped ();
    cmd_vel.header.stamp    = pose.header.stamp;
    cmd_vel.header.frame_id = base_frame_;
    message.clear ();

    if (!core_) {
        message = "LookaheadController: not initialized";
        return ec::kCtrlNotInitialized;
    }
    if (cancel_requested_.load (std::memory_order_relaxed)) {
        message = "LookaheadController: cancelled";
        return ec::kCtrlCancelled;
    }

    last_pose_ = mbc::toPose2D (pose);
    nc::Twist2D out;
    const auto status = core_->computeCommand (last_pose_, out);
    cmd_vel.twist.linear.x  = out.vx;
    cmd_vel.twist.linear.y  = out.vy;
    cmd_vel.twist.angular.z = out.wz;

    // State trace: 20 Hz × 1 in 20 → roughly once per second. Enough to
    // confirm the controller is alive in CI without flooding the log.
    ++compute_calls_;
    if (auto node = node_.lock (); node && (compute_calls_ % 20 == 1)) {
        RCLCPP_INFO (node->get_logger (),
                     "[%s] compute #%zu pose=(%.2f,%.2f,%.2f) "
                     "cmd=(%.2f,%.2f,%.2f) status=%s plan_size=%zu",
                     name_.c_str (), compute_calls_,
                     last_pose_.x, last_pose_.y, last_pose_.yaw,
                     out.vx, out.vy, out.wz,
                     nc::toString (status), cached_path_size_);
    }

    if (status != nc::ControllerResult::Success && status != nc::ControllerResult::GoalReached) {
        message = std::string ("LookaheadController: ") + nc::toString (status);
    }
    return ec::toMbfControllerOutcome (status);
}

bool LookaheadController::isGoalReached (double dist_tolerance, double angle_tolerance) {
    if (!core_) {
        return false;
    }
    const bool reached = core_->isGoalReached (dist_tolerance, angle_tolerance);
    if (reached) {
        if (auto node = node_.lock ()) {
            RCLCPP_INFO (node->get_logger (),
                         "[%s] isGoalReached -> true (d_tol=%.3f a_tol=%.3f)",
                         name_.c_str (), dist_tolerance, angle_tolerance);
        }
    }
    return reached;
}

bool LookaheadController::setPlan (const std::vector<geometry_msgs::msg::PoseStamped> &plan) {
    if (!core_) {
        return false;
    }
    cancel_requested_.store (false, std::memory_order_relaxed);
    auto path_2d = mbc::toPath2D (plan);
    cached_path_size_ = path_2d.poses.size ();
    ++set_plan_calls_;
    if (auto node = node_.lock ()) {
        RCLCPP_INFO (node->get_logger (),
                     "[%s] setPlan #%zu received path with %zu poses",
                     name_.c_str (), set_plan_calls_, cached_path_size_);
    }
    core_->setPlan (std::move (path_2d));
    return true;
}

bool LookaheadController::cancel () {
    cancel_requested_.store (true, std::memory_order_relaxed);
    if (core_) {
        core_->cancel ();
    }
    return true;
}

}  // namespace texnitis::mbf_controllers

PLUGINLIB_EXPORT_CLASS (texnitis::mbf_controllers::LookaheadController,
                        mbf_simple_core::SimpleController)
