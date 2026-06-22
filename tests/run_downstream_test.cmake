if(NOT DEFINED ORBITQUEUE_SOURCE_DIR OR NOT DEFINED ORBITQUEUE_BINARY_DIR)
    message(FATAL_ERROR "OrbitQueue source and binary directories are required")
endif()

set(test_root "${ORBITQUEUE_BINARY_DIR}/downstream-package-test")
set(install_prefix "${test_root}/install")
set(consumer_build "${test_root}/build")

file(REMOVE_RECURSE "${test_root}")

set(config_args)
set(ctest_config_args)
if(DEFINED ORBITQUEUE_TEST_CONFIG AND NOT ORBITQUEUE_TEST_CONFIG STREQUAL "")
    list(APPEND config_args --config "${ORBITQUEUE_TEST_CONFIG}")
    list(APPEND ctest_config_args -C "${ORBITQUEUE_TEST_CONFIG}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${ORBITQUEUE_BINARY_DIR}"
        --prefix "${install_prefix}" ${config_args}
    RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Failed to install OrbitQueue for the downstream test")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -S "${ORBITQUEUE_SOURCE_DIR}/tests/downstream"
        -B "${consumer_build}"
        "-DCMAKE_PREFIX_PATH=${install_prefix}"
        "-DCMAKE_BUILD_TYPE=${ORBITQUEUE_TEST_CONFIG}"
    RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Failed to configure the downstream OrbitQueue consumer")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${consumer_build}" ${config_args}
    RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Failed to build the downstream OrbitQueue consumer")
endif()

execute_process(
    COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${consumer_build}"
        --output-on-failure ${ctest_config_args}
    RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "The downstream OrbitQueue consumer test failed")
endif()
