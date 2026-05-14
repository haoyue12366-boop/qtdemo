cmake_minimum_required(VERSION 3.20)

project(qthmi VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.10 REQUIRED COMPONENTS
    Core
    Gui
    Qml
    Quick
    QuickControls2
    QuickDialogs2
    SerialPort
    Sql
    Concurrent
)

qt_standard_project_setup(REQUIRES 6.10)

qt_add_executable(qthmi
    src/main.cpp
    src/logger.h
    src/logger.cpp
    src/configmanager.h
    src/configmanager.cpp
    src/modbusprotocol.h
    src/modbusprotocol.cpp
    src/alarmevaluator.h
    src/alarmevaluator.cpp
    src/communicationstats.h
    src/backgroundtasktypes.h
    src/backgroundtaskmanager.h
    src/backgroundtaskmanager.cpp
    src/tasks/tasksqlconnection.h
    src/tasks/alarmcsvexporttask.h
    src/tasks/alarmcsvexporttask.cpp
    src/tasks/alarmhistoryquerytask.h
    src/tasks/alarmhistoryquerytask.cpp
    src/tasks/alarmstatstask.h
    src/tasks/alarmstatstask.cpp
    src/serialportworker.h
    src/serialportworker.cpp
    src/alarmrecord.h
    src/alarmrecorddata.h
    src/telemetryrecord.h
    src/alarmhistorymodel.h
    src/alarmhistorymodel.cpp
    src/alarmlogmodel.h
    src/alarmlogmodel.cpp
    src/databaseworker.h
    src/databaseworker.cpp
    src/backend.h
    src/backend.cpp
    src/backend_backgroundtasks.cpp
    src/curveplotprovider.h
    src/curveplotprovider.cpp
)

qt_add_qml_module(qthmi
    URI Qthmi
    VERSION 1.0
    QML_FILES
        qml/ConfigPanel.qml
        qml/TrendPanel.qml
        qml/CommunicationStatsPanel.qml
        qml/AlarmPanel.qml
        qml/TelemetrySummaryPanel.qml
        qml/Main.qml
)

target_link_libraries(qthmi PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::QuickDialogs2
    Qt6::SerialPort
    Qt6::Sql
    Qt6::Concurrent
)

target_include_directories(qthmi PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(MSVC)
    target_compile_options(qthmi PRIVATE /W4 /permissive-)
else()
    target_compile_options(qthmi PRIVATE -Wall -Wextra -Wpedantic)
endif()
