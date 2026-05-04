cmake_minimum_required(VERSION 3.20)
project(texnitis_mbf_sim NONE)

# Pure Python sim package (ament_cmake + ament_cmake_python).
# Module body lives under texnitis_mbf_sim/, executable scripts under scripts/,
# launch+config under launch/ and config/.

find_package(ament_cmake REQUIRED)
find_package(ament_cmake_python REQUIRED)

ament_python_install_package(${PROJECT_NAME})

install(PROGRAMS
    scripts/flat_world_sim_node.py
    scripts/run_e2e_check.py
    DESTINATION lib/${PROJECT_NAME}
)

install(
    DIRECTORY launch config
    DESTINATION share/${PROJECT_NAME}
)

if(BUILD_TESTING)
    find_package(ament_lint_auto REQUIRED)
    ament_lint_auto_find_test_dependencies()

    find_package(ament_cmake_pytest REQUIRED)
    ament_add_pytest_test(test_kinematics test/test_kinematics.py)
endif()

ament_package()
