add_executable(ecu_app_host main.cpp package_reader.cpp)
target_link_libraries(ecu_app_host PRIVATE ECU::app_host)
ecu_app_strict(ecu_app_host)
