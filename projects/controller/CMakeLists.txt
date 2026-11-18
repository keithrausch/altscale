
set(PROJECT_NAME projects_controller)

add_executable(${PROJECT_NAME} main.cpp)

target_link_libraries(${PROJECT_NAME} PUBLIC 
    common
    managers_controller
)