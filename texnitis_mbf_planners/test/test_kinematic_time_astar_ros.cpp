#include "texnitis_mbf_planners/kinematic_time_astar_planner.hpp"

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>
#include <texnitis_mbf_common/error_codes.hpp>
#include <texnitis_navigation_interfaces/msg/terrain_grid.hpp>
#include <texnitis_navigation_interfaces/msg/trajectory2_d.hpp>

#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace {
using namespace std::chrono_literals;
namespace ec = texnitis::mbf_common::error_codes;

class KinematicPlannerRosTest : public ::testing::Test {
   protected:
    static void SetUpTestSuite () {
        if (!rclcpp::ok ()) {
            int argc = 0;
            rclcpp::init (argc, nullptr);
        }
    }

    static void TearDownTestSuite () {
        if (rclcpp::ok ()) rclcpp::shutdown ();
    }

    static nav_msgs::msg::OccupancyGrid openMap () {
        nav_msgs::msg::OccupancyGrid map;
        map.header.frame_id              = "map";
        map.info.width                   = 20;
        map.info.height                  = 20;
        map.info.resolution              = 0.1F;
        map.info.origin.orientation.w    = 1.0;
        map.data.assign (map.info.width * map.info.height, 0);
        return map;
    }

    static geometry_msgs::msg::PoseStamped pose (double x, double y, double yaw = 0.0) {
        geometry_msgs::msg::PoseStamped value;
        value.header.frame_id       = "map";
        value.pose.position.x       = x;
        value.pose.position.y       = y;
        value.pose.orientation.z    = std::sin (yaw * 0.5);
        value.pose.orientation.w    = std::cos (yaw * 0.5);
        return value;
    }

    static void spinFor (const rclcpp::Node::SharedPtr &node, std::chrono::milliseconds duration = 100ms) {
        rclcpp::executors::SingleThreadedExecutor executor;
        executor.add_node (node);
        const auto deadline = std::chrono::steady_clock::now () + duration;
        while (std::chrono::steady_clock::now () < deadline) executor.spin_some ();
        executor.remove_node (node);
    }
};

TEST_F (KinematicPlannerRosTest, returns_not_initialized_before_map_arrives) {
    auto node = std::make_shared<rclcpp::Node> ("planner_without_map");
    texnitis::mbf_planners::KinematicTimeAStarPlanner planner;
    planner.initialize ("planner", node);
    std::vector<geometry_msgs::msg::PoseStamped> plan;
    double cost = 0.0;
    std::string message;

    const auto outcome = planner.makePlan (pose (0.15, 0.15), pose (1.15, 0.15), 0.0, plan, cost, message);

    EXPECT_EQ (outcome, ec::kNotInitialized);
    EXPECT_TRUE (plan.empty ());
    EXPECT_FALSE (message.empty ());
}

TEST_F (KinematicPlannerRosTest, plans_from_plain_occupancy_grid_and_publishes_trajectory) {
    auto node = std::make_shared<rclcpp::Node> ("planner_plain_map");
    texnitis::mbf_planners::KinematicTimeAStarPlanner planner;
    planner.initialize ("planner", node);
    texnitis_navigation_interfaces::msg::Trajectory2D::SharedPtr received;
    auto sub = node->create_subscription<texnitis_navigation_interfaces::msg::Trajectory2D> (
        "planner/trajectory", rclcpp::QoS (1).reliable ().transient_local (),
        [&received] (const texnitis_navigation_interfaces::msg::Trajectory2D::SharedPtr msg) { received = msg; });
    auto publisher = node->create_publisher<nav_msgs::msg::OccupancyGrid> (
        "/map", rclcpp::QoS (1).reliable ().transient_local ());
    publisher->publish (openMap ());
    spinFor (node);
    std::vector<geometry_msgs::msg::PoseStamped> plan;
    double cost = 0.0;
    std::string message;

    const auto outcome = planner.makePlan (pose (0.15, 0.15), pose (1.15, 0.15), 0.0, plan, cost, message);
    spinFor (node);

    ASSERT_EQ (outcome, ec::kSuccess);
    ASSERT_GE (plan.size (), 2U);
    EXPECT_GT (cost, 0.0);
    ASSERT_NE (received, nullptr);
    ASSERT_EQ (received->points.size (), plan.size ());
    int64_t previous_ns = -1;
    for (const auto &point : received->points) {
        const int64_t current_ns =
            static_cast<int64_t> (point.time_from_start.sec) * 1000000000LL +
            point.time_from_start.nanosec;
        EXPECT_GT (current_ns, previous_ns);
        previous_ns = current_ns;
    }
    const auto &last = received->points.back ();
    EXPECT_DOUBLE_EQ (last.velocity.linear.x, 0.0);
    EXPECT_DOUBLE_EQ (last.velocity.linear.y, 0.0);
    EXPECT_DOUBLE_EQ (last.velocity.angular.z, 0.0);
    (void)sub;
}

TEST_F (KinematicPlannerRosTest, returns_not_initialized_when_required_slope_is_missing) {
    rclcpp::NodeOptions options;
    options.parameter_overrides ({
        rclcpp::Parameter ("planner.missing_terrain_policy", "require_slope"),
    });
    auto node = std::make_shared<rclcpp::Node> ("planner_requires_slope", options);
    texnitis::mbf_planners::KinematicTimeAStarPlanner planner;
    planner.initialize ("planner", node);
    auto publisher = node->create_publisher<nav_msgs::msg::OccupancyGrid> (
        "/map", rclcpp::QoS (1).reliable ().transient_local ());
    publisher->publish (openMap ());
    spinFor (node);
    std::vector<geometry_msgs::msg::PoseStamped> plan;
    double cost = 0.0;
    std::string message;

    const auto outcome = planner.makePlan (pose (0.15, 0.15), pose (1.15, 0.15), 0.0, plan, cost, message);

    EXPECT_EQ (outcome, ec::kNotInitialized);
    EXPECT_TRUE (plan.empty ());
}

TEST_F (KinematicPlannerRosTest, plans_when_required_terrain_grid_arrives) {
    rclcpp::NodeOptions options;
    options.parameter_overrides ({
        rclcpp::Parameter ("planner.missing_terrain_policy", "require_slope"),
    });
    auto node = std::make_shared<rclcpp::Node> ("planner_with_slope", options);
    texnitis::mbf_planners::KinematicTimeAStarPlanner planner;
    planner.initialize ("planner", node);
    auto map_publisher = node->create_publisher<nav_msgs::msg::OccupancyGrid> (
        "/map", rclcpp::QoS (1).reliable ().transient_local ());
    auto terrain_publisher =
        node->create_publisher<texnitis_navigation_interfaces::msg::TerrainGrid> (
            "/terrain_grid", rclcpp::QoS (1).reliable ().transient_local ());
    const auto map = openMap ();
    texnitis_navigation_interfaces::msg::TerrainGrid terrain;
    terrain.header.frame_id = "map";
    terrain.info            = map.info;
    const size_t cell_count =
        static_cast<size_t> (terrain.info.width) * terrain.info.height;
    terrain.gradient_x.assign (cell_count, 0.05F);
    terrain.gradient_y.assign (cell_count, 0.0F);
    terrain.valid_mask.assign (cell_count, 2U);
    map_publisher->publish (map);
    terrain_publisher->publish (terrain);
    spinFor (node);
    std::vector<geometry_msgs::msg::PoseStamped> plan;
    double cost = 0.0;
    std::string message;

    const auto outcome = planner.makePlan (
        pose (0.15, 0.15), pose (1.15, 0.15), 0.0, plan, cost, message);

    EXPECT_EQ (outcome, ec::kSuccess);
    EXPECT_GE (plan.size (), 2U);
    EXPECT_GT (cost, 0.0);
}

}  // namespace
