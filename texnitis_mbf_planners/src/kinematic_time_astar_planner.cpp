#include "texnitis_mbf_planners/kinematic_time_astar_planner.hpp"

#include <pluginlib/class_list_macros.hpp>
#include <texnitis_mbf_common/error_codes.hpp>
#include <texnitis_mbf_common/pose_conversions.hpp>
#include <texnitis_mbf_common/ros_logger_bridge.hpp>
#include <texnitis_nav_core/motion_models/differential_drive_motion_model.hpp>
#include <texnitis_nav_core/motion_models/holonomic_motion_model.hpp>

namespace texnitis::mbf_planners {
namespace nc  = ::texnitis::nav_core;
namespace mbc = ::texnitis::mbf_common;
namespace ec  = ::texnitis::mbf_common::error_codes;
namespace {
template <typename T>
T param (const rclcpp::Node::SharedPtr &node, const std::string &name, const T &fallback) {
    if (!node->has_parameter (name)) node->declare_parameter<T> (name, fallback);
    return node->get_parameter (name).get_value<T> ();
}
nc::MissingTerrainPolicy missingPolicy (const std::string &value) {
    if (value == "require_elevation") return nc::MissingTerrainPolicy::RequireElevation;
    if (value == "require_slope") return nc::MissingTerrainPolicy::RequireSlope;
    if (value == "require_all") return nc::MissingTerrainPolicy::RequireAll;
    return nc::MissingTerrainPolicy::Allow2DOnly;
}
}  // namespace

void KinematicTimeAStarPlanner::initialize (const std::string name, const rclcpp::Node::SharedPtr &node) {
    name_                        = name;
    node_                        = node;
    map_topic_                   = param<std::string> (node, name + ".map_topic", "/map");
    terrain_topic_               = param<std::string> (node, name + ".terrain_topic", "/terrain_grid");
    global_frame_                = param<std::string> (node, name + ".global_frame", "map");
    const std::string model_name = param<std::string> (node, name + ".motion_model", "holonomic");

    nc::KinematicTimeAStarParams p;
    p.heading_bins                            = static_cast<int> (param<int64_t> (node, name + ".heading_bins", p.heading_bins));
    p.max_iterations                          = static_cast<int> (param<int64_t> (node, name + ".max_iterations", p.max_iterations));
    p.planning_timeout                        = param<double> (node, name + ".planning_timeout", p.planning_timeout);
    p.heuristic_weight                        = param<double> (node, name + ".heuristic_weight", p.heuristic_weight);
    p.goal_yaw_tolerance                      = param<double> (node, name + ".goal_yaw_tolerance", p.goal_yaw_tolerance);
    p.map_preprocessor.occupied_threshold     = static_cast<int> (param<int64_t> (node, name + ".occupied_threshold", p.map_preprocessor.occupied_threshold));
    p.map_preprocessor.unknown_is_obstacle    = param<bool> (node, name + ".unknown_is_obstacle", p.map_preprocessor.unknown_is_obstacle);
    p.map_preprocessor.inflation_radius       = param<double> (node, name + ".inflation_radius", p.map_preprocessor.inflation_radius);
    p.map_preprocessor.max_step_height        = param<double> (node, name + ".max_step_height", p.map_preprocessor.max_step_height);
    p.map_preprocessor.max_slope              = param<double> (node, name + ".max_slope", p.map_preprocessor.max_slope);
    p.map_preprocessor.slope_cost_scale       = param<double> (node, name + ".slope_cost_scale", p.map_preprocessor.slope_cost_scale);
    p.map_preprocessor.missing_terrain_policy = missingPolicy (param<std::string> (node, name + ".missing_terrain_policy", "allow_2d_only"));
    p.terrain_vehicle.uphill_speed_scale_per_grade = param<double> (node, name + ".uphill_speed_scale_per_grade", p.terrain_vehicle.uphill_speed_scale_per_grade);
    p.terrain_vehicle.downhill_speed_scale_per_grade = param<double> (node, name + ".downhill_speed_scale_per_grade", p.terrain_vehicle.downhill_speed_scale_per_grade);
    p.terrain_vehicle.minimum_speed_scale = param<double> (node, name + ".minimum_terrain_speed_scale", p.terrain_vehicle.minimum_speed_scale);
    p.terrain_vehicle.maximum_longitudinal_grade = param<double> (node, name + ".maximum_longitudinal_grade", p.terrain_vehicle.maximum_longitudinal_grade);
    p.terrain_vehicle.track_width = param<double> (node, name + ".track_width", p.terrain_vehicle.track_width);
    p.terrain_vehicle.center_of_gravity_height = param<double> (node, name + ".center_of_gravity_height", p.terrain_vehicle.center_of_gravity_height);
    p.terrain_vehicle.rollover_safety_factor = param<double> (node, name + ".rollover_safety_factor", p.terrain_vehicle.rollover_safety_factor);
    p.terrain_vehicle.require_valid_slope = param<bool> (node, name + ".require_valid_slope", p.terrain_vehicle.require_valid_slope);

    std::shared_ptr<const nc::MotionModel> model;
    if (model_name == "differential_drive") {
        nc::DifferentialDriveMotionModelParams m;
        m.heading_bins             = p.heading_bins;
        m.primitive_length         = param<double> (node, name + ".primitive_length", m.primitive_length);
        m.max_linear_velocity      = param<double> (node, name + ".max_linear_velocity", m.max_linear_velocity);
        m.max_linear_acceleration  = param<double> (node, name + ".max_linear_acceleration", m.max_linear_acceleration);
        m.max_angular_velocity     = param<double> (node, name + ".max_angular_velocity", m.max_angular_velocity);
        m.max_angular_acceleration = param<double> (node, name + ".max_angular_acceleration", m.max_angular_acceleration);
        m.minimum_turning_radius   = param<double> (node, name + ".minimum_turning_radius", m.minimum_turning_radius);
        m.allow_in_place_rotation  = param<bool> (node, name + ".allow_in_place_rotation", m.allow_in_place_rotation);
        m.allow_reverse            = param<bool> (node, name + ".allow_reverse", m.allow_reverse);
        model                      = std::make_shared<nc::DifferentialDriveMotionModel> (m);
    } else {
        nc::HolonomicMotionModelParams m;
        m.heading_bins             = p.heading_bins;
        m.primitive_length         = param<double> (node, name + ".primitive_length", m.primitive_length);
        m.max_velocity_x           = param<double> (node, name + ".max_velocity_x", m.max_velocity_x);
        m.max_velocity_y           = param<double> (node, name + ".max_velocity_y", m.max_velocity_y);
        m.max_acceleration_x       = param<double> (node, name + ".max_acceleration_x", m.max_acceleration_x);
        m.max_acceleration_y       = param<double> (node, name + ".max_acceleration_y", m.max_acceleration_y);
        m.max_angular_velocity     = param<double> (node, name + ".max_angular_velocity", m.max_angular_velocity);
        m.max_angular_acceleration = param<double> (node, name + ".max_angular_acceleration", m.max_angular_acceleration);
        model                      = std::make_shared<nc::HolonomicMotionModel> (m);
    }
    core_ = std::make_unique<nc::KinematicTimeAStarPlanner> (p, model);
    core_->setLogger (mbc::makeRosLoggerBridge (node->get_logger ().get_child (name)));
    map_provider_     = mbc::MapProvider::get (node);
    terrain_provider_ = mbc::TerrainMapProvider::get (node);
    map_provider_->subscribe (map_topic_);
    terrain_provider_->subscribe (terrain_topic_);
    plan_pub_ = node->create_publisher<nav_msgs::msg::Path> (name + "/plan", rclcpp::QoS (1).reliable ().transient_local ());
    trajectory_pub_ = node->create_publisher<texnitis_navigation_interfaces::msg::Trajectory2D> (name + "/trajectory", rclcpp::QoS (1).reliable ().transient_local ());
}

uint32_t KinematicTimeAStarPlanner::makePlan (const geometry_msgs::msg::PoseStamped &start, const geometry_msgs::msg::PoseStamped &goal, double, std::vector<geometry_msgs::msg::PoseStamped> &plan, double &cost, std::string &message) {
    plan.clear ();
    cost = 0.0;
    message.clear ();
    auto map_msg = map_provider_ ? map_provider_->latest (map_topic_) : nullptr;
    if (!core_ || !map_msg) {
        message = "planner or map not initialized";
        return ec::kNotInitialized;
    }
    nc::TerrainLayersView layers;
    auto                  terrain_msg = terrain_provider_ ? terrain_provider_->latest (terrain_topic_) : nullptr;
    if (terrain_msg) {
        const size_t   total     = static_cast<size_t> (terrain_msg->info.width) * terrain_msg->info.height;
        const uint8_t *mask      = terrain_msg->valid_mask.size () == total ? terrain_msg->valid_mask.data () : nullptr;
        const auto     make_view = [&] (const auto &values, uint8_t required_bit) {
            nc::FloatGridView view;
            view.data                = values.data ();
            view.width               = static_cast<int> (terrain_msg->info.width);
            view.height              = static_cast<int> (terrain_msg->info.height);
            view.resolution          = terrain_msg->info.resolution;
            view.origin_x            = terrain_msg->info.origin.position.x;
            view.origin_y            = terrain_msg->info.origin.position.y;
            view.valid_mask          = mask;
            view.required_valid_bits = required_bit;
            return view;
        };
        if (terrain_msg->elevation_m.size () == total) layers.elevation = make_view (terrain_msg->elevation_m, 1U);
        if (terrain_msg->gradient_x.size () == total && terrain_msg->gradient_y.size () == total) layers.slope = nc::SlopeGridView{make_view (terrain_msg->gradient_x, 2U), make_view (terrain_msg->gradient_y, 2U)};
    }
    nc::Trajectory2D trajectory;
    const auto result = core_->planTrajectoryWithTerrain (mbc::toGridMapView (*map_msg), layers, mbc::toPose2D (start), mbc::toPose2D (goal), trajectory);
    if (result != nc::PlanResult::Success) {
        message = std::string ("KinematicTimeAStarPlanner: ") + nc::toString (result);
        return ec::toMbfPlannerOutcome (result);
    }
    auto               node  = node_.lock ();
    const rclcpp::Time stamp = node ? node->now () : rclcpp::Time (0);
    nc::Path2D path;
    for (const auto &point : trajectory.points) path.poses.push_back (point.pose);
    plan = mbc::toPoseStampedVector (path, global_frame_, stamp);
    cost                     = core_->lastPlanCost ();
    if (plan_pub_) {
        nav_msgs::msg::Path msg;
        msg.header.frame_id = global_frame_;
        msg.header.stamp    = stamp;
        msg.poses           = plan;
        plan_pub_->publish (msg);
    }
    if (trajectory_pub_) {
        texnitis_navigation_interfaces::msg::Trajectory2D msg;
        msg.header.frame_id = global_frame_;
        msg.header.stamp = stamp;
        msg.points.reserve (trajectory.points.size ());
        for (const auto &source : trajectory.points) {
            texnitis_navigation_interfaces::msg::TrajectoryPoint2D point;
            point.pose.position.x = source.pose.x;
            point.pose.position.y = source.pose.y;
            point.pose.orientation.z = std::sin (source.pose.yaw * 0.5);
            point.pose.orientation.w = std::cos (source.pose.yaw * 0.5);
            point.velocity.linear.x = source.velocity.vx;
            point.velocity.linear.y = source.velocity.vy;
            point.velocity.angular.z = source.velocity.wz;
            point.acceleration.linear.x = source.acceleration.vx;
            point.acceleration.linear.y = source.acceleration.vy;
            point.acceleration.angular.z = source.acceleration.wz;
            const auto nanoseconds = static_cast<int64_t> (std::llround (source.time_from_start * 1.0e9));
            point.time_from_start.sec = static_cast<int32_t> (nanoseconds / 1000000000LL);
            point.time_from_start.nanosec = static_cast<uint32_t> (nanoseconds % 1000000000LL);
            msg.points.push_back (point);
        }
        trajectory_pub_->publish (msg);
    }
    return ec::kSuccess;
}

bool KinematicTimeAStarPlanner::cancel () {
    if (core_) core_->cancel ();
    return true;
}

}  // namespace texnitis::mbf_planners

PLUGINLIB_EXPORT_CLASS (texnitis::mbf_planners::KinematicTimeAStarPlanner, mbf_simple_core::SimplePlanner)
