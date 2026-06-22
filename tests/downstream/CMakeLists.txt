cmake_minimum_required(VERSION 3.20)

project(OrbitQueueDownstream LANGUAGES CXX)

find_package(OrbitQueue 2 CONFIG REQUIRED)

add_executable(orbitqueue_downstream main.cpp)
target_link_libraries(orbitqueue_downstream PRIVATE OrbitQueue::orbitqueue)

enable_testing()
add_test(NAME orbitqueue_downstream COMMAND orbitqueue_downstream)
