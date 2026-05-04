/// @file
/// @brief MecanumMppiController mbf アダプタ。`mbf_simple_core::
///        SimpleController` を継承して core にすべて委譲する。

#pragma once

#include <atomic>
#include <memory>
#include <string>

#include <geometry_msgs/msg/pose_stamped.hpp>
#include <geometry_msgs/msg/twist_stamped.hpp>
#include <mbf_simple_core/simple_controller.h>
#include <rclcpp/rclcpp.hpp>
#include <tf2_ros/buffer.h>

#include <texnitis_nav_core/mppi/mecanum_mppi_controller.hpp>

namespace texnitis::mbf_controllers {

class MecanumMppiController final : public mbf_simple_core::SimpleController {
   public:
    MecanumMppiController ()           = default;
    ~MecanumMppiController () override = default;

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
    std::unique_ptr<::texnitis::nav_core::mppi::MecanumMppiController>            core_;

    std::string base_frame_{"base_link"};

    std::atomic<bool> cancel_requested_{false};
};

}  // namespace texnitis::mbf_controllers
