cmake_minimum_required(VERSION 3.20)
project(texnitis_mbf_bringup NONE)

# Pure launch / config package. No build steps; install share/ contents only.
# Real launch / config files land in milestones M3..M6.

find_package(ament_cmake REQUIRED)

install(
    DIRECTORY launch config
    DESTINATION share/${PROJECT_NAME}
)

if(BUILD_TESTING)
    find_package(ament_lint_auto REQUIRED)
    ament_lint_auto_find_test_dependencies()
endif()

ament_package()
