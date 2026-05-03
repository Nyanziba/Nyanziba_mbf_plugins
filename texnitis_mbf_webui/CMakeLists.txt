cmake_minimum_required(VERSION 3.20)
project(texnitis_mbf_webui NONE)

# Static asset package. webui/ には HTML / JS / CSS、launch/ には rosbridge と
# 静的ファイル配信用 launch を入れる。実装は M6。

find_package(ament_cmake REQUIRED)

install(
    DIRECTORY webui launch
    DESTINATION share/${PROJECT_NAME}
)

if(BUILD_TESTING)
    find_package(ament_lint_auto REQUIRED)
    ament_lint_auto_find_test_dependencies()
endif()

ament_package()
