find_package(Threads REQUIRED)

add_executable(orbitqueue_tests
    test_main.cpp
    fixed_message_tests.cpp
    blocking_queue_tests.cpp
    spsc_queue_tests.cpp
    spmc_multicast_queue_tests.cpp)
target_link_libraries(orbitqueue_tests PRIVATE orbitqueue Threads::Threads)

add_test(NAME orbitqueue_tests COMMAND orbitqueue_tests)

add_test(
    NAME orbitqueue_downstream_package
    COMMAND "${CMAKE_COMMAND}"
        "-DORBITQUEUE_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DORBITQUEUE_BINARY_DIR=${PROJECT_BINARY_DIR}"
        "-DORBITQUEUE_TEST_CONFIG=$<CONFIG>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_downstream_test.cmake")
