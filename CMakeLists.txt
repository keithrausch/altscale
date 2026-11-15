cmake_minimum_required(VERSION 3.14)

project(project_scale LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#
# functions
#

macro(add_all_subdirectories)
    file(GLOB SUBDIRS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" CONFIGURE_DEPENDS "*")

    foreach(SUBDIR ${SUBDIRS})
        if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${SUBDIR}" AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${SUBDIR}/CMakeLists.txt")
            add_subdirectory("${SUBDIR}")
        endif()
    endforeach()
endmacro()

add_subdirectory(external)
add_subdirectory(common)
add_subdirectory(managers)
add_subdirectory(projects)