
include(FetchContent)

set(BOOST_INCLUDE_LIBRARIES asio system beast)
set(BOOST_ENABLE_CMAKE ON)

FetchContent_Declare(
    Boost
    GIT_REPOSITORY https://github.com/boostorg/boost.git
    GIT_TAG        boost-1.86.0
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(Boost)