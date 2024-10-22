cmake_minimum_required(VERSION 3.14)
project(ast_matcher_5 LANGUAGES CXX C)

# 设置 CMAKE_MODULE_PATH 指向包含 FindClangFoo.cmake 的目录
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../../cmake" ${CMAKE_MODULE_PATH})

include(CheckCXXCompilerFlag)
include(Sanitizers)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
set(CMAKE_CXX_STANDARD 20)

# 使用 FindClangFoo.cmake 查找 ClangFoo
find_package(ClangFoo REQUIRED)
include(CheckStdFormat)
import_std_format()

list(APPEND all_targets tool)
add_executable(tool)
target_sources(tool PRIVATE main.cpp)
target_link_libraries(tool PRIVATE ClangFoo::llvm ClangFoo::clangcpp)