add_library(ecu_app_sdk INTERFACE)
add_library(ECU::app_sdk ALIAS ecu_app_sdk)
target_include_directories(ecu_app_sdk INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(ecu_app_sdk INTERFACE cxx_std_17)
