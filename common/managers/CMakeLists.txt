
set(LIB_NAME common_managers)

set(${LIB_NAME}_HEADERS
    manager_base.hpp
    manager_runner.hpp
    )

set(${LIB_NAME}_SOURCE
    )

# TODO turn on warnings

add_library(${LIB_NAME} ${${LIB_NAME}_HEADERS} ${${LIB_NAME}_SOURCE})
set_target_properties(${LIB_NAME} PROPERTIES LINKER_LANGUAGE CXX)

target_include_directories(${LIB_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)