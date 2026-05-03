cmake_minimum_required(VERSION 3.20)
project(texnitis_mbf_controllers CXX)

# Scaffold only. Real targets land in milestones M3 (Lookahead),
# M4 (DiffDrivePurePursuit) and M5 (MecanumMPPI).

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)

find_package(geometry_msgs QUIET)
find_package(nav_msgs QUIET)
find_package(pluginlib QUIET)
find_package(rclcpp QUIET)
find_package(tf2 QUIET)
find_package(tf2_geometry_msgs QUIET)
find_package(tf2_ros QUIET)
find_package(mbf_abstract_core QUIET)
find_package(mbf_simple_core QUIET)
find_package(mbf_msgs QUIET)
find_package(mbf_utility QUIET)
find_package(texnitis_mbf_common QUIET)
find_package(texnitis_nav_core QUIET CONFIG)

install(FILES plugins.xml DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
    find_package(ament_lint_auto REQUIRED)
    ament_lint_auto_find_test_dependencies()
endif()

ament_package()
