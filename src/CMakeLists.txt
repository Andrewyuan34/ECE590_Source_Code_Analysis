cmake_minimum_required(VERSION 3.14)
project(ast_matcher_5 LANGUAGES CXX C)

# 设置 CMAKE_MODULE_PATH 指向包含 FindClangFoo.cmake 的目录
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../cmake" ${CMAKE_MODULE_PATH})

include(CheckCXXCompilerFlag)
include(Sanitizers)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
set(CMAKE_CXX_STANDARD 20)

# 使用 FindClangFoo.cmake 查找 ClangFoo
find_package(ClangFoo REQUIRED)
include(CheckStdFormat)
import_std_format()

set(CMAKE_CXX_COMPILER "clang++")
set(CMAKE_C_COMPILER "clang")

list(APPEND all_targets tool)
add_executable(tool)
target_sources(tool PRIVATE main.cpp MatchCallback.cpp)
target_link_libraries(tool PRIVATE ClangFoo::llvm ClangFoo::clangcpp)

# 在 CMakeLists.txt 的末尾输出编译器选择
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(STATUS "Final Compiler Selection: Using Clang as the compiler.")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    message(STATUS "Final Compiler Selection: Using GCC as the compiler.")
else()
    message(STATUS "Final Compiler Selection: Using an unsupported compiler.")
endif()