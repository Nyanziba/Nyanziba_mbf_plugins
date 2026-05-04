"""Launch the static WebUI alongside rosbridge_server.

Starts:
  - rosbridge_websocket on port 9090
  - a Python http.server hosting webui/ on the chosen port (default 8000)
"""

from __future__ import annotations

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, ExecuteProcess
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description() -> LaunchDescription:
    """Build the launch description."""
    pkg_share = FindPackageShare("texnitis_mbf_webui")
    webui_dir = PathJoinSubstitution([pkg_share, "webui"])

    http_port = LaunchConfiguration("http_port")
    rosbridge_port = LaunchConfiguration("rosbridge_port")

    return LaunchDescription(
        [
            DeclareLaunchArgument(
                "http_port",
                default_value="8000",
                description="Port for the static HTTP server.",
            ),
            DeclareLaunchArgument(
                "rosbridge_port",
                default_value="9090",
                description="Port for rosbridge_websocket.",
            ),
            Node(
                package="rosbridge_server",
                executable="rosbridge_websocket",
                name="rosbridge_websocket",
                parameters=[{"port": rosbridge_port}],
                output="screen",
            ),
            ExecuteProcess(
                cmd=["python3", "-m", "http.server", http_port, "--directory", webui_dir],
                output="screen",
            ),
        ]
    )
