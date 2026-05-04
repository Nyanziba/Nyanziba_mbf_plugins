/// @file
/// @brief mbf_simple_core::SimplePlanner adapter that delegates to
///        texnitis_nav_core::AStarPlanner.
///
/// 設計方針:
///  - アダプタは「ROS 型⇄コア POD 型」の変換と、ROS パラメータから
///    `nav_core::AStarParams` への充填だけを担当する。アルゴリズムは
///    一切ここに書かない。
///  - /map は `MapProvider` シングルトン経由で共有購読する。
///  - cancel() は core 側 atomic flag を立て、`true` を返す。

#pragma once

#include <atomic>
#include <memory>
#include <string>

#include <mbf_simple_core/simple_planner.h>
#include <rclcpp/rclcpp.hpp>

#include <texnitis_mbf_common/map_provider.hpp>
#include <texnitis_nav_core/planners/astar_planner.hpp>

namespace texnitis::mbf_planners {

class AStarPlanner final : public mbf_simple_core::SimplePlanner {
   public:
    AStarPlanner ()           = default;
    ~AStarPlanner () override = default;

    void initialize (const std::string             name,
                     const rclcpp::Node::SharedPtr &node_handle) override;

    uint32_t makePlan (const geometry_msgs::msg::PoseStamped               &start,
                       const geometry_msgs::msg::PoseStamped               &goal,
                       double                                                tolerance,
                       std::vector<geometry_msgs::msg::PoseStamped>         &plan,
                       double                                                &cost,
                       std::string                                           &message) override;

    bool cancel () override;

   private:
    std::string                                  name_;
    rclcpp::Node::WeakPtr                        node_;
    std::unique_ptr<::texnitis::nav_core::AStarPlanner> core_;
    std::shared_ptr<::texnitis::mbf_common::MapProvider> map_provider_;

    std::string map_topic_{"/map"};
    std::string global_frame_{"map"};

    std::atomic<bool> cancel_requested_{false};
};

}  // namespace texnitis::mbf_planners
