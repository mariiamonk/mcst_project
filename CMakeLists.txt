cmake_minimum_required(VERSION 3.25.1)
project(cache_project VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(COMMON_SOURCES src/cache.cpp src/memory.cpp)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")
    add_executable(${PROJECT_NAME} src/main.cpp ${COMMON_SOURCES})
    target_include_directories(${PROJECT_NAME} PRIVATE include)
endif()

add_executable(model1 src/model1.cpp ${COMMON_SOURCES})
target_include_directories(model1 PRIVATE include)

add_executable(model2 src/model2.cpp ${COMMON_SOURCES})
target_include_directories(model2 PRIVATE include)