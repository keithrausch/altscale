
set(LIB_NAME common_types)

set(${LIB_NAME}_HEADERS
    example.hpp
    )

set(${LIB_NAME}_SOURCE
    )

# TODO turn on warnings

add_library(${LIB_NAME} ${${LIB_NAME}_HEADERS} ${${LIB_NAME}_SOURCE})
set_target_properties(${LIB_NAME} PROPERTIES LINKER_LANGUAGE CXX)

target_include_directories(${LIB_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_link_libraries(${LIB_NAME} PUBLIC 
    common_utils # SEE ME. common::types links to common::utils. CAREFUL OF CIRCULAR LINKAGE
)

