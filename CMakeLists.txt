cmake_minimum_required(VERSION 3.25.1)
project(cache_project VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED COMPONENTS program_options)

set(COMMON_SOURCES src/cache.cpp src/memory.cpp)
set(COMMON_INCLUDES include)

add_executable(cache_project src/main.cpp ${COMMON_SOURCES})
target_include_directories(cache_project PRIVATE ${COMMON_INCLUDES})
target_link_libraries(cache_project PRIVATE Boost::program_options)

add_executable(model1 src/model1.cpp ${COMMON_SOURCES})
target_include_directories(model1 PRIVATE ${COMMON_INCLUDES})
target_link_libraries(model1 PRIVATE Boost::program_options)

add_executable(model2 src/model2.cpp ${COMMON_SOURCES})
target_include_directories(model2 PRIVATE ${COMMON_INCLUDES})
target_link_libraries(model2 PRIVATE Boost::program_options)

enable_testing()
add_test(NAME test1 COMMAND model1 --test ${CMAKE_CURRENT_SOURCE_DIR}/tests/test1.txt --trace 3)
add_test(NAME test2 COMMAND model2 --test ${CMAKE_CURRENT_SOURCE_DIR}/tests/test2.txt --trace 3)