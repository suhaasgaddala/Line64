find_package(Threads REQUIRED)

add_executable(orbitqueue_benchmark queue_benchmark.cpp)
target_link_libraries(orbitqueue_benchmark PRIVATE orbitqueue Threads::Threads)
target_include_directories(orbitqueue_benchmark PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

execute_process(
    COMMAND git rev-parse --short=12 HEAD
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    RESULT_VARIABLE orbitqueue_git_result
    OUTPUT_VARIABLE orbitqueue_git_commit
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
if(NOT orbitqueue_git_result EQUAL 0 OR orbitqueue_git_commit STREQUAL "")
    set(orbitqueue_git_commit "unknown")
endif()

target_compile_definitions(orbitqueue_benchmark PRIVATE
    "ORBITQUEUE_BENCHMARK_BUILD_TYPE=\"$<CONFIG>\""
    "ORBITQUEUE_BENCHMARK_COMPILER=\"${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}\""
    "ORBITQUEUE_BENCHMARK_GIT_COMMIT=\"${orbitqueue_git_commit}\"")

if(ORBITQUEUE_ENABLE_BOOST_BENCHMARKS)
    find_path(ORBITQUEUE_BOOST_LOCKFREE_INCLUDE_DIR
        NAMES boost/lockfree/queue.hpp)
    find_path(ORBITQUEUE_BOOST_LOCKFREE_SPSC_INCLUDE_DIR
        NAMES boost/lockfree/spsc_queue.hpp)

    if(ORBITQUEUE_BOOST_LOCKFREE_INCLUDE_DIR)
        target_compile_definitions(orbitqueue_benchmark PRIVATE
            ORBITQUEUE_HAVE_BOOST_LOCKFREE_QUEUE=1)
        target_include_directories(orbitqueue_benchmark SYSTEM PRIVATE
            "${ORBITQUEUE_BOOST_LOCKFREE_INCLUDE_DIR}")
        message(STATUS "Bounded queue Boost.Lockfree MPMC benchmarks enabled")
    else()
        message(WARNING
            "ORBITQUEUE_ENABLE_BOOST_BENCHMARKS is ON, but "
            "boost/lockfree/queue.hpp was not found; Boost MPMC scenarios are disabled")
    endif()

    if(ORBITQUEUE_BOOST_LOCKFREE_SPSC_INCLUDE_DIR)
        target_compile_definitions(orbitqueue_benchmark PRIVATE
            ORBITQUEUE_HAVE_BOOST_LOCKFREE_SPSC=1)
        target_include_directories(orbitqueue_benchmark SYSTEM PRIVATE
            "${ORBITQUEUE_BOOST_LOCKFREE_SPSC_INCLUDE_DIR}")
        message(STATUS "Bounded queue Boost.Lockfree SPSC benchmarks enabled")
    else()
        message(WARNING
            "ORBITQUEUE_ENABLE_BOOST_BENCHMARKS is ON, but "
            "boost/lockfree/spsc_queue.hpp was not found; Boost SPSC scenarios are disabled")
    endif()
endif()

if(ORBITQUEUE_ENABLE_MOODYCAMEL_BENCHMARKS OR
   ORBITQUEUE_ENABLE_RIGTORP_BENCHMARKS OR
   ORBITQUEUE_ENABLE_ATOMIC_QUEUE_BENCHMARKS)
    include(FetchContent)
endif()

if(ORBITQUEUE_ENABLE_MOODYCAMEL_BENCHMARKS)
    FetchContent_Declare(orbitqueue_moodycamel_concurrentqueue
        GIT_REPOSITORY https://github.com/cameron314/concurrentqueue.git
        GIT_TAG 9afb99746f0f5fc94ac8aef737053ae0481ba8d1
        SOURCE_SUBDIR cmake/line64-header-only)
    FetchContent_GetProperties(orbitqueue_moodycamel_concurrentqueue)
    if(NOT orbitqueue_moodycamel_concurrentqueue_POPULATED)
        FetchContent_MakeAvailable(orbitqueue_moodycamel_concurrentqueue)
    endif()
    target_compile_definitions(orbitqueue_benchmark PRIVATE
        ORBITQUEUE_HAVE_MOODYCAMEL_CONCURRENTQUEUE=1)
    target_include_directories(orbitqueue_benchmark SYSTEM PRIVATE
        "${orbitqueue_moodycamel_concurrentqueue_SOURCE_DIR}")
    message(STATUS "Bounded queue moodycamel::ConcurrentQueue benchmarks enabled")
endif()

if(ORBITQUEUE_ENABLE_RIGTORP_BENCHMARKS)
    FetchContent_Declare(orbitqueue_rigtorp_spscqueue
        GIT_REPOSITORY https://github.com/rigtorp/SPSCQueue.git
        GIT_TAG 565a5149d54930463d58cb0f69b978d439555e66
        SOURCE_SUBDIR cmake/line64-header-only)
    FetchContent_GetProperties(orbitqueue_rigtorp_spscqueue)
    if(NOT orbitqueue_rigtorp_spscqueue_POPULATED)
        FetchContent_MakeAvailable(orbitqueue_rigtorp_spscqueue)
    endif()
    target_compile_definitions(orbitqueue_benchmark PRIVATE
        ORBITQUEUE_HAVE_RIGTORP_SPSCQUEUE=1)
    target_include_directories(orbitqueue_benchmark SYSTEM PRIVATE
        "${orbitqueue_rigtorp_spscqueue_SOURCE_DIR}/include")
    message(STATUS "Bounded queue rigtorp::SPSCQueue benchmarks enabled")
endif()

if(ORBITQUEUE_ENABLE_ATOMIC_QUEUE_BENCHMARKS)
    FetchContent_Declare(orbitqueue_atomic_queue
        GIT_REPOSITORY https://github.com/max0x7ba/atomic_queue.git
        GIT_TAG c3eeb3d7bf85e8a6414294f5510681b7a595f97c
        SOURCE_SUBDIR cmake/line64-header-only)
    FetchContent_GetProperties(orbitqueue_atomic_queue)
    if(NOT orbitqueue_atomic_queue_POPULATED)
        FetchContent_MakeAvailable(orbitqueue_atomic_queue)
    endif()
    target_compile_definitions(orbitqueue_benchmark PRIVATE
        ORBITQUEUE_HAVE_ATOMIC_QUEUE=1)
    target_include_directories(orbitqueue_benchmark SYSTEM PRIVATE
        "${orbitqueue_atomic_queue_SOURCE_DIR}/include")
    message(STATUS "Bounded queue atomic_queue benchmarks enabled")
endif()

if(ORBITQUEUE_BUILD_TESTS)
    add_test(NAME orbitqueue_benchmark_smoke
        COMMAND orbitqueue_benchmark
            --duration-ms 10 --warmup-ms 0 --trials 1
            --capacity 64 --payload-size 32 --consumers 1,3 --queue all)
endif()
