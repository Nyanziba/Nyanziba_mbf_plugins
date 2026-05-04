"""Launch the move_base_flex (simple_nav) stack with texnitis adapters.

Loads the YAML at config/texnitis_mbf.yaml and starts a single
mbf_simple_nav node that picks up our planner / controller plugins via
pluginlib. Map publication and TF are expected to come from a sibling
launch (line_map_publisher.launch.xml in the legacy bringup, or any
custom equivalent).
"""

from __future__ import annotations

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description() -> LaunchDescription:
    """Build the launch description."""
    pkg_share = FindPackageShare("texnitis_mbf_bringup")
    default_params = PathJoinSubstitution([pkg_share, "config", "texnitis_mbf.yaml"])

    use_sim_time = LaunchConfiguration("use_sim_time")
    params_file = LaunchConfiguration("params_file")

    return LaunchDescription(
        [
            DeclareLaunchArgument(
                "use_sim_time",
                default_value="false",
                description="Use /clock if true.",
            ),
            DeclareLaunchArgument(
                "params_file",
                default_value=default_params,
                description="Override path to the bringup YAML.",
            ),
            Node(
                package="mbf_simple_nav",
                executable="mbf_simple_nav",
                name="move_base_flex",
                output="screen",
                parameters=[
                    params_file,
                    {"use_sim_time": use_sim_time},
                ],
            ),
        ]
    )
