cmake_minimum_required(VERSION 3.8)

project(diagnostic_remote_logging)
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(dependencies
    ament_cmake
    rclcpp
    diagnostic_msgs
    CURL
    )

foreach(dep ${dependencies})
    find_package(${dep} REQUIRED)
endforeach(dep)

include_directories(
    src/
    include/
)

add_executable(telegraf
                src/telegraf.cpp
                )

ament_target_dependencies(telegraf ${dependencies})

ament_export_dependencies(telegraf ${dependencies})

target_compile_features(telegraf PUBLIC c_std_99 cxx_std_17)

install(TARGETS telegraf
         DESTINATION lib/${PROJECT_NAME}
         )

ament_package()
