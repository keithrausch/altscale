

set(LIB_NAME common)

add_subdirectory(managers)
add_subdirectory(utils)

add_library(${LIB_NAME} INTERFACE)


target_include_directories(${LIB_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(${LIB_NAME} INTERFACE
    common_managers
    common_utils
)