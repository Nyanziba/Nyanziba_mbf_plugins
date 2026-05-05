cmake_minimum_required(VERSION 3.20)
project(texnitis_mbf_tools NONE)

# ament_cmake で Python パッケージを扱う構成。
# Python モジュールは texnitis_mbf_tools/ 以下、エントリポイント用スクリプトは
# scripts/ 配下に置き、`ros2 run texnitis_mbf_tools <name>` で起動できるよう
# lib/${PROJECT_NAME}/ に install する。
#
# 実装は M6 で追加。現時点ではパッケージのみ install し、スクリプト install は
# コメントアウトしておく。

find_package(ament_cmake REQUIRED)
find_package(ament_cmake_python REQUIRED)

ament_python_install_package(${PROJECT_NAME})

install(PROGRAMS
    scripts/waypoint_sender.py
    scripts/mbf_action_bridge.py
    scripts/nav_state_publisher.py
    scripts/rviz_goal_bridge.py
    DESTINATION lib/${PROJECT_NAME}
)

if(BUILD_TESTING)
    find_package(ament_lint_auto REQUIRED)
    ament_lint_auto_find_test_dependencies()
endif()

ament_package()
