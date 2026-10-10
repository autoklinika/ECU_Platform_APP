# Explicit source list: adding a test requires review, not a wildcard.
set(ECU_CORE_TEST_SOURCES
  core_v2_cyclic_can_actuator_runtime_tests.cpp
  core_v2_dut_neutral_shared_bus_tests.cpp
  core_v2_foundation_tests.cpp
  core_v2_isobus_etp_tests.cpp
  core_v2_isobus_network_management_tests.cpp
  core_v2_isobus_working_set_lifecycle_tests.cpp
  core_v2_isobus_working_set_tests.cpp
  core_v2_isotp_tests.cpp
  core_v2_j1939_acknowledgment_tests.cpp
  core_v2_j1939_diagnostics_tests.cpp
  core_v2_j1939_fd_cpg_tests.cpp
  core_v2_j1939_fd_tp_tests.cpp
  core_v2_j1939_network_tests.cpp
  core_v2_j1939_request_tests.cpp
  core_v2_j1939_tp_e2e_tests.cpp
  core_v2_j1939_tp_tests.cpp
  core_v2_j1939_tp_tx_tests.cpp
  core_v2_runtime_foundation_tests.cpp
  core_v2_uds_isotp_e2e_tests.cpp
  core_v2_uds_tests.cpp
)
set(ECU_CORE_TEST_TARGETS)
foreach(source IN LISTS ECU_CORE_TEST_SOURCES)
  get_filename_component(stem "${source}" NAME_WE)
  set(target "ecu_${stem}")
  add_executable(${target} "${source}")
  target_link_libraries(${target} PRIVATE ECU::core_v2)
  set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX /permissive- /Zc:__cplusplus)
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  string(REGEX REPLACE "^core_v2_(.*)_tests$" "\\1" suffix "${stem}")
  add_test(NAME "ecu.core_v2.${suffix}" COMMAND ${target})
  set_tests_properties("ecu.core_v2.${suffix}" PROPERTIES TIMEOUT 60 LABELS "core;offline")
  list(APPEND ECU_CORE_TEST_TARGETS ${target})
endforeach()
add_custom_target(ecu_core_v2_tests DEPENDS ${ECU_CORE_TEST_TARGETS})

add_test(NAME ecu.repo.core_import
  COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/verify_core_import.py")
add_test(NAME ecu.repo.core_architecture
  COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/scripts/check_core_v2_architecture.py" "${PROJECT_SOURCE_DIR}/src/core_v2")
add_test(NAME ecu.repo.guard_regression
  COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/repo/test_guards.py")
set_tests_properties(ecu.repo.core_import ecu.repo.core_architecture ecu.repo.guard_regression
  PROPERTIES TIMEOUT 120 LABELS "repository;offline")

# The host is independent of Core at this step. Each target has its own boundary.
if(ECU_APP_BUILD_HOST)
  add_executable(ecu_app_manifest_tests host/manifest_tests.cpp)
  target_link_libraries(ecu_app_manifest_tests PRIVATE ECU::app_host)
  ecu_app_strict(ecu_app_manifest_tests)
  add_test(NAME ecu.app.manifest COMMAND ecu_app_manifest_tests)

  add_executable(ecu_app_host_tests host/host_tests.cpp)
  target_link_libraries(ecu_app_host_tests PRIVATE ECU::app_host)
  ecu_app_strict(ecu_app_host_tests)
  add_test(NAME ecu.app.host COMMAND ecu_app_host_tests)

  add_executable(ecu_app_sdk_compile_test host/sdk_compile_test.cpp)
  target_link_libraries(ecu_app_sdk_compile_test PRIVATE ECU::app_sdk)
  ecu_app_strict(ecu_app_sdk_compile_test)
  add_test(NAME ecu.app.sdk_standalone COMMAND ecu_app_sdk_compile_test)

  add_test(NAME ecu.app.cli
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/host/test_cli.py" "$<TARGET_FILE:ecu_app_host>")
  add_test(NAME ecu.app.boundaries
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/host/test_boundaries.py")
  set_tests_properties(ecu.app.manifest ecu.app.host ecu.app.sdk_standalone ecu.app.cli ecu.app.boundaries
    PROPERTIES TIMEOUT 120 LABELS "app;offline")
endif()

if(ECU_APP_BUILD_HOST)
  add_executable(ecu_app_runtime_tests runtime/runtime_tests.cpp)
  target_link_libraries(ecu_app_runtime_tests PRIVATE ECU::app_runtime ECU::app_mock_support)
  ecu_app_strict(ecu_app_runtime_tests)
  foreach(scenario lifecycle ownership cancellation timeouts faults bounds timestamps)
    add_test(NAME ecu.runtime.${scenario} COMMAND ecu_app_runtime_tests ${scenario})
    set_tests_properties(ecu.runtime.${scenario} PROPERTIES TIMEOUT 30 LABELS "runtime;offline;mock")
  endforeach()
  add_executable(ecu_app_host_runtime_tests runtime/host_runtime_tests.cpp)
  target_link_libraries(ecu_app_host_runtime_tests PRIVATE ECU::app_host ECU::app_runtime ECU::app_mock_support)
  ecu_app_strict(ecu_app_host_runtime_tests)
  add_test(NAME ecu.runtime.host_integration COMMAND ecu_app_host_runtime_tests)
  set_tests_properties(ecu.runtime.host_integration PROPERTIES TIMEOUT 30 LABELS "runtime;offline;mock")
endif()

if(TARGET ecu_runtime_mock)
  add_test(NAME ecu.runtime.mock_cli
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/runtime/test_mock_cli.py" "$<TARGET_FILE:ecu_runtime_mock>")
  set_tests_properties(ecu.runtime.mock_cli PROPERTIES TIMEOUT 60 LABELS "runtime;offline;mock")
endif()
