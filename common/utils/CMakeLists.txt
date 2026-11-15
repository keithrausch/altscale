
set(LIB_NAME common_utils)

find_package(Threads REQUIRED)

set(${LIB_NAME}_HEADERS
    main_helper.hpp
    signal.hpp
    singleton.hpp
    sleeper.hpp
    xml.hpp
    )

set(${LIB_NAME}_SOURCE
    )

# TODO turn on warnings

add_library(${LIB_NAME} ${${LIB_NAME}_HEADERS} ${${LIB_NAME}_SOURCE})
set_target_properties(${LIB_NAME} PROPERTIES LINKER_LANGUAGE CXX)

target_include_directories(${LIB_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(${LIB_NAME} PUBLIC 
    Threads::Threads
    rapidxml
)

