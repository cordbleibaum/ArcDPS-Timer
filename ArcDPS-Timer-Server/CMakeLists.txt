cmake_minimum_required(VERSION 3.13)
project(arcdps-timer-server)

set(CMAKE_CXX_STANDARD 23)

find_package(nlohmann_json_schema_validator REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Boost  REQUIRED)
if (Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
endif ()

add_executable(arcdps-timer-server main.cpp group.cpp server.cpp session.cpp)
target_link_libraries(arcdps-timer-server PRIVATE nlohmann_json_schema_validator nlohmann_json::nlohmann_json)