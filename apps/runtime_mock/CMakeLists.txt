add_executable(ecu_runtime_mock main.cpp ../host/package_reader.cpp)
target_include_directories(ecu_runtime_mock PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../host)
target_link_libraries(ecu_runtime_mock PRIVATE ECU::app_host ECU::app_runtime ECU::app_mock_support)
ecu_app_strict(ecu_runtime_mock)
