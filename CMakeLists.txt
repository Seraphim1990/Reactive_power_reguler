cmake_minimum_required(VERSION 3.16)
project(ReactivePowerService LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Network WebSockets SerialBus)

add_executable(ReactivePowerService
    main.cpp
    MyModbus/MOdbusDataUnit.h MyModbus/MOdbusDataUnit.cpp
    MyModbus/ModbusReply.h MyModbus/ModbusReply.cpp
    MyModbus/MyModbysTCP.h MyModbus/MyModbysTCP.cpp
    APIWorker/ApiWorker.h APIWorker/ApiWorker.cpp
    ModbusReguler/ModbusReguler.h ModbusReguler/ModbusReguler.cpp
    ModbusReguler/CondensatorsWorker.h ModbusReguler/CondensatorsWorker.cpp
)

target_link_libraries(ReactivePowerService PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::WebSockets
    Qt6::SerialBus
)

include(GNUInstallDirs)
install(TARGETS ReactivePowerService
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
