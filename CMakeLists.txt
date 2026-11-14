cmake_minimum_required(VERSION 3.14)

project(project_scale LANGUAGES CXX)
set(PROJECT_NAME_RENDEVOUZ rendevouz_server)
set(PROJECT_NAME_CLIENT client_control_plane)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# httplib
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib
    GIT_TAG        v0.66.0 # Replace with your preferred release tag
)
FetchContent_MakeAvailable(httplib)

# nlohmann
FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json
    GIT_TAG v3.12.0
)
FetchContent_MakeAvailable(json)

# rendevouz server
add_executable(${PROJECT_NAME_RENDEVOUZ} main_rendevouz.cpp sleeper.hpp)
target_link_libraries(${PROJECT_NAME_RENDEVOUZ} PRIVATE 
    httplib::httplib 
    nlohmann_json::nlohmann_json
)

# client control plane
add_executable(${PROJECT_NAME_CLIENT} main_controller.cpp sleeper.hpp)
target_link_libraries(${PROJECT_NAME_CLIENT} PRIVATE 
    httplib::httplib 
    nlohmann_json::nlohmann_json
)
