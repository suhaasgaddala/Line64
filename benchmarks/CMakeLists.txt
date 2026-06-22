find_package(Threads REQUIRED)

add_executable(orbitqueue_benchmark queue_benchmark.cpp)
target_link_libraries(orbitqueue_benchmark PRIVATE orbitqueue Threads::Threads)
