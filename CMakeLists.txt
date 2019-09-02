cmake_minimum_required(VERSION 3.0.0)
project(feverrpc VERSION 0.1.0)

# include(CTest)

# c++17
set(CMAKE_CXX_STANDARD 17)

find_package(GTest REQUIRED)
find_package(Threads REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})
include_directories(include)

add_executable(feverrpc main.cpp)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})

target_link_libraries(feverrpc ${GTEST_BOTH_LIBRARIES})
target_link_libraries(feverrpc ${CMAKE_THREAD_LIBS_INIT})

# include(CPack)

include_directories("msgpack/include/")

add_test(Test feverrpc)
enable_testing()
