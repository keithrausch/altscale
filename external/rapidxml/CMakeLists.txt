
include(FetchContent)

FetchContent_Declare(
  rapidxml
  GIT_REPOSITORY https://github.com/discord/rapidxml
  GIT_TAG        2ae4b2888165a393dfb6382168825fddf00c27b9 # 1.13
)

FetchContent_MakeAvailable(rapidxml)

# rapidxml does not contain its own CMakeLists file. we have to manually create a target

add_library(rapidxml INTERFACE)
target_include_directories(rapidxml INTERFACE "${rapidxml_SOURCE_DIR}")