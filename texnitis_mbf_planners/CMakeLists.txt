cmake_minimum_required(VERSION 3.20)
project(texnitis_mbf_planners CXX)

# Milestone M3: AStarPlanner adapter that delegates to texnitis_nav_core.

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(pluginlib REQUIRED)
find_package(rclcpp REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_geometry_msgs REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(mbf_abstract_core REQUIRED)
find_package(mbf_simple_core REQUIRED)
find_package(mbf_msgs REQUIRED)
find_package(mbf_utility REQUIRED)
find_package(texnitis_mbf_common REQUIRED)
find_package(texnitis_nav_core REQUIRED CONFIG)

add_library(texnitis_mbf_planners SHARED
    src/astar_planner.cpp
)

target_include_directories(texnitis_mbf_planners
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

ament_target_dependencies(texnitis_mbf_planners
    PUBLIC
        rclcpp
        geometry_msgs
        nav_msgs
        pluginlib
        tf2
        tf2_geometry_msgs
        tf2_ros
        mbf_abstract_core
        mbf_simple_core
        mbf_msgs
        mbf_utility
        texnitis_mbf_common
)

target_link_libraries(texnitis_mbf_planners
    PRIVATE
        texnitis_nav_core::texnitis_nav_core
)

pluginlib_export_plugin_description_file(mbf_simple_core plugins.xml)

install(DIRECTORY include/
    DESTINATION include
)

install(
    TARGETS texnitis_mbf_planners
    EXPORT texnitis_mbf_plannersTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(FILES plugins.xml DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
    find_package(ament_lint_auto REQUIRED)
    ament_lint_auto_find_test_dependencies()
endif()

ament_export_targets(texnitis_mbf_plannersTargets HAS_LIBRARY_TARGET)
ament_export_include_directories(include)
ament_export_dependencies(
    rclcpp geometry_msgs nav_msgs pluginlib tf2 tf2_geometry_msgs tf2_ros
    mbf_abstract_core mbf_simple_core mbf_msgs mbf_utility texnitis_mbf_common
)

ament_package()
