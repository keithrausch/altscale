
set(LIB_NAME managers_controller)

set(${LIB_NAME}_HEADERS
    manager.hpp
    )

set(${LIB_NAME}_SOURCE
    manager.cpp
    )

# TODO turn on warnings

add_library(${LIB_NAME} ${${LIB_NAME}_HEADERS} ${${LIB_NAME}_SOURCE})
set_target_properties(${LIB_NAME} PROPERTIES LINKER_LANGUAGE CXX)

target_include_directories(${LIB_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)

target_link_libraries(${LIB_NAME} PUBLIC 
    common
    Boost::asio 
    Boost::system
    Boost::beast
    )