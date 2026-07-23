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

find_package(geometry_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(rclcpp REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_geometry_msgs REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(mbf_msgs QUIET)
find_package(texnitis_nav_core REQUIRED CONFIG)
find_package(texnitis_navigation_interfaces REQUIRED)

add_library(texnitis_mbf_common SHARED
    src/map_provider.cpp
    src/terrain_map_provider.cpp
)

target_include_directories(texnitis_mbf_common
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

ament_target_dependencies(texnitis_mbf_common
    PUBLIC
        rclcpp
        geometry_msgs
        nav_msgs
        tf2
        tf2_geometry_msgs
        tf2_ros
        texnitis_navigation_interfaces
)

target_link_libraries(texnitis_mbf_common
    PUBLIC
        texnitis_nav_core::texnitis_nav_core
)

install(
    DIRECTORY include/
    DESTINATION include
)

install(
    TARGETS texnitis_mbf_common
    EXPORT texnitis_mbf_commonTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

if(BUILD_TESTING)
    find_package(ament_lint_auto REQUIRED)
    ament_lint_auto_find_test_dependencies()
endif()

ament_export_targets(texnitis_mbf_commonTargets HAS_LIBRARY_TARGET)
ament_export_include_directories(include)
ament_export_dependencies(rclcpp geometry_msgs nav_msgs tf2 tf2_geometry_msgs tf2_ros texnitis_nav_core texnitis_navigation_interfaces)

ament_package()
