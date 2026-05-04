/// @file
/// @brief DiffDrivePurePursuitController mbf アダプタ。core が
///        加速度上限と stateful GoalChecker を持つので、アダプタ層
///        は ROS 型変換とパラメータ宣言だけ。

#pragma once

#include <atomic>
#include <memory>
#include <string>

#include <geometry_msgs/msg/pose_stamped.hpp>
#include <geometry_msgs/msg/twist_stamped.hpp>
#include <mbf_simple_core/simple_controller.h>
#include <rclcpp/rclcpp.hpp>
#include <tf2_ros/buffer.h>

#include <texnitis_nav_core/controllers/diff_drive_pure_pursuit_controller.hpp>

namespace texnitis::mbf_controllers {

class DiffDrivePurePursuitController final : public mbf_simple_core::SimpleController {
   public:
    DiffDrivePurePursuitController ()           = default;
    ~DiffDrivePurePursuitController () override = default;

    void initialize (const std::string                  name,
                     const std::shared_ptr<::TF>       &tf,
                     const rclcpp::Node::SharedPtr     &node_handle) override;

    uint32_t computeVelocityCommands (const geometry_msgs::msg::PoseStamped  &pose,
                                       const geometry_msgs::msg::TwistStamped &velocity,
                                       geometry_msgs::msg::TwistStamped       &cmd_vel,
                                       std::string                            &message) override;

    bool isGoalReached (double dist_tolerance, double angle_tolerance) override;

    bool setPlan (const std::vector<geometry_msgs::msg::PoseStamped> &plan) override;

    bool cancel () override;

   private:
    std::string                                                                  name_;
    rclcpp::Node::WeakPtr                                                        node_;
    std::shared_ptr<::TF>                                                        tf_;
    std::unique_ptr<::texnitis::nav_core::DiffDrivePurePursuitController>         core_;

    std::string base_frame_{"base_link"};

    std::atomic<bool> cancel_requested_{false};
};

}  // namespace texnitis::mbf_controllers
