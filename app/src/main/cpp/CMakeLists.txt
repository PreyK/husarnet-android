cmake_minimum_required(VERSION 3.22.1)
project(husarnet_android C CXX)

# husarnet.cmake adds -fsanitize=undefined for Debug, which doesn't link on the NDK
set(CMAKE_BUILD_TYPE Release)

list(PREPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

set(HUSARNET_ROOT ${CMAKE_CURRENT_LIST_DIR}/../../../../third_party/husarnet)
include(${HUSARNET_ROOT}/core/husarnet.cmake)

add_library(husarnet_jni SHARED husarnet_jni.cpp)
target_compile_definitions(husarnet_jni PRIVATE ETL_ARRAY_VIEW_IS_MUTABLE=1)
target_link_libraries(husarnet_jni husarnet_core log)
