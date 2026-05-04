/// @file
/// @brief mbf_simple_core::SimpleController adapter delegating to
///        texnitis_nav_core::LookaheadController.
///
/// `setPlan(plan)` で受け取った経路を `nav_core::Path2D` に変換して
/// core にキャッシュさせる。`computeVelocityCommands` は毎 tick
/// `nav_core::ControllerBase::computeCommand` を呼び、結果の `Twist2D`
/// を `TwistStamped` に詰めて返す。

#pragma once

#include <atomic>
#include <memory>
#include <string>

#include <geometry_msgs/msg/pose_stamped.hpp>
#include <geometry_msgs/msg/twist_stamped.hpp>
#include <mbf_simple_core/simple_controller.h>
#include <rclcpp/rclcpp.hpp>
#include <tf2_ros/buffer.h>

#include <texnitis_nav_core/controllers/lookahead_controller.hpp>

namespace texnitis::mbf_controllers {

class LookaheadController final : public mbf_simple_core::SimpleController {
   public:
    LookaheadController ()           = default;
    ~LookaheadController () override = default;

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
    std::string                                              name_;
    rclcpp::Node::WeakPtr                                    node_;
    std::shared_ptr<::TF>                                    tf_;
    std::unique_ptr<::texnitis::nav_core::LookaheadController> core_;

    std::string base_frame_{"base_link"};

    /// 直近の current_pose を `isGoalReached` が `setPlan` 経由で
    /// 拾うためのキャッシュ。`computeVelocityCommands` で毎回更新。
    ::texnitis::nav_core::Pose2D last_pose_{};

    std::atomic<bool> cancel_requested_{false};
};

}  // namespace texnitis::mbf_controllers
