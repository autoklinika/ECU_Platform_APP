add_library(ecu_app_mock_support INTERFACE)
add_library(ECU::app_mock_support ALIAS ecu_app_mock_support)
target_include_directories(ecu_app_mock_support INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(ecu_app_mock_support INTERFACE ECU::core_v2)
