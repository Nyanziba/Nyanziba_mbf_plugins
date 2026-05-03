cmake_minimum_required(VERSION 3.20)
project(texnitis_mbf_common CXX)

# Scaffold only. Real targets land in milestone M3.

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)

find_package(geometry_msgs QUIET)
find_package(nav_msgs QUIET)
find_package(rclcpp QUIET)
find_package(tf2 QUIET)
find_package(tf2_geometry_msgs QUIET)
find_package(tf2_ros QUIET)
find_package(mbf_msgs QUIET)
find_package(texnitis_nav_core QUIET CONFIG)

install(
    DIRECTORY include/
    DESTINATION include
)

if(BUILD_TESTING)
    find_package(ament_lint_auto REQUIRED)
    ament_lint_auto_find_test_dependencies()
endif()

ament_export_include_directories(include)

ament_package()
