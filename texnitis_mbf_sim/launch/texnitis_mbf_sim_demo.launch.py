"""Headless ROS 2 sim demo: map + sim + mbf + e2e check.

Brings up:
  1. nav2_map_server with the demo corridor map
  2. nav2_lifecycle_manager (configures + activates the map server)
  3. flat_world_sim (this package): /cmd_vel -> /odom + tf
  4. mbf_simple_nav with the texnitis adapters (texnitis_mbf_bringup)
  5. mbf_e2e_check: sends one goal, exits when result returns

When `e2e_check:=true` (the default), the e2e check node has
on_exit=Shutdown so the whole launch shuts down once it returns,
which is what CI needs. Set `e2e_check:=false` for an interactive
session that keeps everything alive (you can then drive goals
manually with rviz2 or `ros2 run texnitis_mbf_tools waypoint_sender.py`).
"""

from __future__ import annotations

from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    IncludeLaunchDescription,
    RegisterEventHandler,
    Shutdown,
)
from launch.conditions import IfCondition
from launch.event_handlers import OnProcessExit
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue
from launch_ros.substitutions import FindPackageShare


def generate_launch_description() -> LaunchDescription:
    bringup_share = FindPackageShare("texnitis_mbf_bringup")
    sim_share = FindPackageShare("texnitis_mbf_sim")

    map_yaml = LaunchConfiguration("map_yaml")
    bringup_yaml = LaunchConfiguration("bringup_yaml")
    e2e_check = LaunchConfiguration("e2e_check")
    goal_x = LaunchConfiguration("goal_x")
    goal_y = LaunchConfiguration("goal_y")
    goal_yaw = LaunchConfiguration("goal_yaw")
    overall_timeout_s = LaunchConfiguration("overall_timeout_s")

    declare_map_yaml = DeclareLaunchArgument(
        "map_yaml",
        default_value=PathJoinSubstitution(
            [sim_share, "config", "corridor.yaml"]
        ),
        description="nav2_map_server YAML to publish on /map",
    )
    declare_bringup_yaml = DeclareLaunchArgument(
        "bringup_yaml",
        default_value=PathJoinSubstitution(
            [bringup_share, "config", "texnitis_mbf.yaml"]
        ),
        description="mbf bringup parameters",
    )
    declare_e2e = DeclareLaunchArgument(
        "e2e_check",
        default_value="true",
        description="Run the one-shot e2e check and shutdown when it exits.",
    )
    declare_goal_x = DeclareLaunchArgument("goal_x", default_value="3.0")
    declare_goal_y = DeclareLaunchArgument("goal_y", default_value="0.5")
    declare_goal_yaw = DeclareLaunchArgument("goal_yaw", default_value="0.0")
    declare_timeout = DeclareLaunchArgument(
        "overall_timeout_s",
        default_value="90.0",
        description="Hard deadline for the e2e check (seconds).",
    )

    rviz_bridge = LaunchConfiguration("rviz_bridge")
    declare_rviz_bridge = DeclareLaunchArgument(
        "rviz_bridge",
        default_value="false",
        description=(
            "Spawn rviz_goal_bridge: forwards rviz2 '2D Goal Pose' "
            "(/goal_pose) to /move_base_flex/move_base."
        ),
    )

    map_server = Node(
        package="nav2_map_server",
        executable="map_server",
        name="map_server",
        output="screen",
        parameters=[{"yaml_filename": map_yaml}],
    )

    lifecycle_manager = Node(
        package="nav2_lifecycle_manager",
        executable="lifecycle_manager",
        name="lifecycle_manager_map",
        output="screen",
        parameters=[
            {"autostart": True, "node_names": ["map_server"]},
        ],
    )

    sim_node = Node(
        package="texnitis_mbf_sim",
        executable="flat_world_sim_node.py",
        name="flat_world_sim",
        output="screen",
        parameters=[
            {"update_rate": 50.0, "initial_pose": [0.1, 0.1, 1.3]},
        ],
    )

    mbf_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            PathJoinSubstitution([bringup_share, "launch", "texnitis_mbf.launch.py"])
        ),
        launch_arguments={"params_file": bringup_yaml}.items(),
    )

    e2e_node = Node(
        package="texnitis_mbf_sim",
        executable="run_e2e_check.py",
        name="mbf_e2e_check",
        output="screen",
        condition=IfCondition(e2e_check),
        # LaunchConfiguration returns strings; force-type each scalar so
        # rclpy declares the parameter as DOUBLE rather than rejecting the
        # string value.
        parameters=[
            {
                "goal_x": ParameterValue(goal_x, value_type=float),
                "goal_y": ParameterValue(goal_y, value_type=float),
                "goal_yaw": ParameterValue(goal_yaw, value_type=float),
                "overall_timeout_s": ParameterValue(
                    overall_timeout_s, value_type=float
                ),
            }
        ],
    )

    shutdown_on_check_exit = RegisterEventHandler(
        OnProcessExit(
            target_action=e2e_node,
            on_exit=[Shutdown(reason="e2e check completed")],
        ),
        condition=IfCondition(e2e_check),
    )

    rviz_goal_bridge_node = Node(
        package="texnitis_mbf_tools",
        executable="rviz_goal_bridge.py",
        name="rviz_goal_bridge",
        output="screen",
        condition=IfCondition(rviz_bridge),
        parameters=[
            {
                "goal_topic": "/goal_pose",
                "action_name": "/move_base_flex/move_base",
                "planner": "astar",
                "controller": "lookahead",
                "cancel_on_new": True,
            }
        ],
    )

    return LaunchDescription(
        [
            declare_map_yaml,
            declare_bringup_yaml,
            declare_e2e,
            declare_goal_x,
            declare_goal_y,
            declare_goal_yaw,
            declare_timeout,
            declare_rviz_bridge,
            map_server,
            lifecycle_manager,
            sim_node,
            mbf_launch,
            e2e_node,
            shutdown_on_check_exit,
            rviz_goal_bridge_node,
        ]
    )
