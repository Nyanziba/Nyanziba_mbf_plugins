// MecanumMppiController mbf adapter implementation.

#include "texnitis_mbf_controllers/mecanum_mppi_controller.hpp"

#include <pluginlib/class_list_macros.hpp>

#include <texnitis_mbf_common/error_codes.hpp>
#include <texnitis_mbf_common/pose_conversions.hpp>
#include <texnitis_mbf_common/ros_logger_bridge.hpp>

namespace texnitis::mbf_controllers {

namespace ec  = ::texnitis::mbf_common::error_codes;
namespace mbc = ::texnitis::mbf_common;
namespace nc  = ::texnitis::nav_core;
namespace nm  = ::texnitis::nav_core::mppi;

namespace {

template <typename T>
T declareOrGet (const rclcpp::Node::SharedPtr &node, const std::string &full_name, const T &default_value) {
    if (!node->has_parameter (full_name)) {
        node->declare_parameter<T> (full_name, default_value);
    }
    return node->get_parameter (full_name).get_value<T> ();
}

}  // namespace

void MecanumMppiController::initialize (const std::string                  name,
                                          const std::shared_ptr<::TF>       &tf,
                                          const rclcpp::Node::SharedPtr     &node_handle) {
    name_       = name;
    node_       = node_handle;
    tf_         = tf;
    base_frame_ = declareOrGet<std::string> (node_handle, name + ".base_frame", std::string ("base_link"));

    nm::MppiParams params;
    params.horizon     = static_cast<int> (declareOrGet<int64_t> (
        node_handle, name + ".horizon", static_cast<int64_t> (params.horizon)));
    params.num_samples = static_cast<int> (declareOrGet<int64_t> (
        node_handle, name + ".num_samples", static_cast<int64_t> (params.num_samples)));
    params.lambda      = declareOrGet<double> (node_handle, name + ".lambda", params.lambda);
    params.dt          = declareOrGet<double> (node_handle, name + ".dt", params.dt);
    params.w_track     = declareOrGet<double> (node_handle, name + ".w_track", params.w_track);
    params.w_control   = declareOrGet<double> (node_handle, name + ".w_control", params.w_control);
    params.w_yaw       = declareOrGet<double> (node_handle, name + ".w_yaw", params.w_yaw);
    params.v_max       = declareOrGet<double> (node_handle, name + ".v_max", params.v_max);
    params.omega_max   = declareOrGet<double> (node_handle, name + ".omega_max", params.omega_max);
    params.wheel_base  = declareOrGet<double> (node_handle, name + ".wheel_base", params.wheel_base);
    params.seed        = static_cast<uint64_t> (declareOrGet<int64_t> (
        node_handle, name + ".seed", static_cast<int64_t> (params.seed)));

    const std::vector<double> sigma_default{params.sigma (0), params.sigma (1), params.sigma (2)};
    const std::vector<double> u_max_default{params.u_max (0), params.u_max (1), params.u_max (2)};
    const auto sigma_v = declareOrGet<std::vector<double>> (node_handle, name + ".sigma", sigma_default);
    const auto u_max_v = declareOrGet<std::vector<double>> (node_handle, name + ".u_max", u_max_default);
    if (sigma_v.size () == 3) {
        params.sigma << sigma_v[0], sigma_v[1], sigma_v[2];
    }
    if (u_max_v.size () == 3) {
        params.u_max << u_max_v[0], u_max_v[1], u_max_v[2];
    }

    params.goal_checker.xy_tolerance  = declareOrGet<double> (
        node_handle, name + ".goal_xy_tolerance", params.goal_checker.xy_tolerance);
    params.goal_checker.yaw_tolerance = declareOrGet<double> (
        node_handle, name + ".goal_yaw_tolerance", params.goal_checker.yaw_tolerance);
    params.goal_checker.stateful      = declareOrGet<bool> (
        node_handle, name + ".goal_stateful", params.goal_checker.stateful);

    core_ = std::make_unique<nm::MecanumMppiController> (params);
    core_->setLogger (mbc::makeRosLoggerBridge (node_handle->get_logger ().get_child (name)));

    RCLCPP_INFO (node_handle->get_logger (),
                 "MecanumMppiController '%s' initialized (horizon=%d samples=%d)",
                 name_.c_str (), params.horizon, params.num_samples);
}

uint32_t MecanumMppiController::computeVelocityCommands (
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
        message = "MecanumMppiController: not initialized";
        return ec::kCtrlNotInitialized;
    }
    if (cancel_requested_.load (std::memory_order_relaxed)) {
        message = "MecanumMppiController: cancelled";
        return ec::kCtrlCancelled;
    }

    const auto current_2d = mbc::toPose2D (pose);
    nc::Twist2D out;
    const auto status = core_->computeCommand (current_2d, out);
    cmd_vel.twist.linear.x  = out.vx;
    cmd_vel.twist.linear.y  = out.vy;
    cmd_vel.twist.angular.z = out.wz;
    if (status != nc::ControllerResult::Success && status != nc::ControllerResult::GoalReached) {
        message = std::string ("MecanumMppiController: ") + nc::toString (status);
    }
    return ec::toMbfControllerOutcome (status);
}

bool MecanumMppiController::isGoalReached (double dist_tolerance, double angle_tolerance) {
    if (!core_) {
        return false;
    }
    return core_->isGoalReached (dist_tolerance, angle_tolerance);
}

bool MecanumMppiController::setPlan (const std::vector<geometry_msgs::msg::PoseStamped> &plan) {
    if (!core_) {
        return false;
    }
    cancel_requested_.store (false, std::memory_order_relaxed);
    core_->setPlan (mbc::toPath2D (plan));
    return true;
}

bool MecanumMppiController::cancel () {
    cancel_requested_.store (true, std::memory_order_relaxed);
    if (core_) {
        core_->cancel ();
    }
    return true;
}

}  // namespace texnitis::mbf_controllers

PLUGINLIB_EXPORT_CLASS (texnitis::mbf_controllers::MecanumMppiController,
                        mbf_simple_core::SimpleController)
