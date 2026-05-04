// DiffDrivePurePursuitController mbf adapter implementation.

#include "texnitis_mbf_controllers/diff_drive_pure_pursuit_controller.hpp"

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

void DiffDrivePurePursuitController::initialize (const std::string                  name,
                                                  const std::shared_ptr<::TF>       &tf,
                                                  const rclcpp::Node::SharedPtr     &node_handle) {
    name_ = name;
    node_ = node_handle;
    tf_   = tf;
    base_frame_ = declareOrGet<std::string> (node_handle, name + ".base_frame", std::string ("base_link"));

    nc::DiffDrivePurePursuitParams params;
    params.max_linear_velocity      = declareOrGet<double> (node_handle, name + ".max_linear_velocity", params.max_linear_velocity);
    params.min_linear_velocity      = declareOrGet<double> (node_handle, name + ".min_linear_velocity", params.min_linear_velocity);
    params.max_angular_velocity     = declareOrGet<double> (node_handle, name + ".max_angular_velocity", params.max_angular_velocity);
    params.max_acceleration         = declareOrGet<double> (node_handle, name + ".max_acceleration", params.max_acceleration);
    params.max_angular_acceleration = declareOrGet<double> (node_handle, name + ".max_angular_acceleration", params.max_angular_acceleration);
    params.lookahead_time           = declareOrGet<double> (node_handle, name + ".lookahead_time", params.lookahead_time);
    params.min_lookahead_distance   = declareOrGet<double> (node_handle, name + ".min_lookahead_distance", params.min_lookahead_distance);
    params.max_lookahead_distance   = declareOrGet<double> (node_handle, name + ".max_lookahead_distance", params.max_lookahead_distance);
    params.angle_speed_p            = declareOrGet<double> (node_handle, name + ".angle_speed_p", params.angle_speed_p);
    params.control_dt               = declareOrGet<double> (node_handle, name + ".control_dt", params.control_dt);

    params.goal_checker.xy_tolerance  = declareOrGet<double> (
        node_handle, name + ".goal_xy_tolerance", params.goal_checker.xy_tolerance);
    params.goal_checker.yaw_tolerance = declareOrGet<double> (
        node_handle, name + ".goal_yaw_tolerance", params.goal_checker.yaw_tolerance);
    params.goal_checker.stateful      = declareOrGet<bool> (
        node_handle, name + ".goal_stateful", params.goal_checker.stateful);

    core_ = std::make_unique<nc::DiffDrivePurePursuitController> (params);
    core_->setLogger (mbc::makeRosLoggerBridge (node_handle->get_logger ().get_child (name)));

    RCLCPP_INFO (node_handle->get_logger (),
                 "DiffDrivePurePursuitController '%s' initialized",
                 name_.c_str ());
}

uint32_t DiffDrivePurePursuitController::computeVelocityCommands (
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
        message = "DiffDrivePurePursuitController: not initialized";
        return ec::kCtrlNotInitialized;
    }
    if (cancel_requested_.load (std::memory_order_relaxed)) {
        message = "DiffDrivePurePursuitController: cancelled";
        return ec::kCtrlCancelled;
    }

    const auto current_2d = mbc::toPose2D (pose);
    nc::Twist2D out;
    const auto status = core_->computeCommand (current_2d, out);
    cmd_vel.twist.linear.x  = out.vx;
    cmd_vel.twist.linear.y  = out.vy;
    cmd_vel.twist.angular.z = out.wz;
    if (status != nc::ControllerResult::Success && status != nc::ControllerResult::GoalReached) {
        message = std::string ("DiffDrivePurePursuitController: ") + nc::toString (status);
    }
    return ec::toMbfControllerOutcome (status);
}

bool DiffDrivePurePursuitController::isGoalReached (double dist_tolerance, double angle_tolerance) {
    if (!core_) {
        return false;
    }
    return core_->isGoalReached (dist_tolerance, angle_tolerance);
}

bool DiffDrivePurePursuitController::setPlan (const std::vector<geometry_msgs::msg::PoseStamped> &plan) {
    if (!core_) {
        return false;
    }
    cancel_requested_.store (false, std::memory_order_relaxed);
    core_->setPlan (mbc::toPath2D (plan));
    return true;
}

bool DiffDrivePurePursuitController::cancel () {
    cancel_requested_.store (true, std::memory_order_relaxed);
    if (core_) {
        core_->cancel ();
    }
    return true;
}

}  // namespace texnitis::mbf_controllers

PLUGINLIB_EXPORT_CLASS (texnitis::mbf_controllers::DiffDrivePurePursuitController,
                        mbf_simple_core::SimpleController)
