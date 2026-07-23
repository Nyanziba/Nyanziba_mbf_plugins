cmake_minimum_required(VERSION 3.20)
project(texnitis_navigation_interfaces)

find_package(ament_cmake REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(std_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(builtin_interfaces REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/TerrainGrid.msg"
  "msg/TrajectoryPoint2D.msg"
  "msg/Trajectory2D.msg"
  DEPENDENCIES std_msgs nav_msgs geometry_msgs builtin_interfaces
)

ament_export_dependencies(rosidl_default_runtime)
ament_package()
