
include(FetchContent)

FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib
    GIT_TAG        v0.66.0 # Replace with your preferred release tag
)

FetchContent_MakeAvailable(httplib)