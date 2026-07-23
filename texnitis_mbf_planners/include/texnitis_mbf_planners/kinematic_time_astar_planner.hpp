#pragma once

#include <rclcpp/rclcpp.hpp>
#include <texnitis_mbf_common/map_provider.hpp>
#include <texnitis_mbf_common/terrain_map_provider.hpp>
#include <texnitis_nav_core/planners/kinematic_time_astar_planner.hpp>

#include <nav_msgs/msg/path.hpp>
#include <texnitis_navigation_interfaces/msg/trajectory2_d.hpp>

#include <mbf_simple_core/simple_planner.h>

#include <memory>
#include <string>

namespace texnitis::mbf_planners {

class KinematicTimeAStarPlanner final : public mbf_simple_core::SimplePlanner {
   public:
    void     initialize (const std::string name, const rclcpp::Node::SharedPtr &node) override;
    uint32_t makePlan (const geometry_msgs::msg::PoseStamped &start, const geometry_msgs::msg::PoseStamped &goal, double tolerance, std::vector<geometry_msgs::msg::PoseStamped> &plan, double &cost, std::string &message) override;
    bool     cancel () override;

   private:
    std::string                                                      name_;
    std::string                                                      map_topic_{"/map"};
    std::string                                                      terrain_topic_{"/terrain_grid"};
    std::string                                                      global_frame_{"map"};
    rclcpp::Node::WeakPtr                                            node_;
    std::unique_ptr<::texnitis::nav_core::KinematicTimeAStarPlanner> core_;
    std::shared_ptr<::texnitis::mbf_common::MapProvider>             map_provider_;
    std::shared_ptr<::texnitis::mbf_common::TerrainMapProvider>      terrain_provider_;
    rclcpp::Publisher<nav_msgs::msg::Path>::SharedPtr                plan_pub_;
    rclcpp::Publisher<texnitis_navigation_interfaces::msg::Trajectory2D>::SharedPtr trajectory_pub_;
};

}  // namespace texnitis::mbf_planners
