cmake_minimum_required(VERSION 3.21)
project(BmsHostApp VERSION 1.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core Quick Qml SerialBus Charts Widgets)

qt_add_executable(BmsHostApp
    main.cpp
    # CAN layer
    can/CanWorker.h can/CanWorker.cpp
    # Protocol layer
    protocol/BmsProtocolDecoder.h protocol/BmsProtocolDecoder.cpp
    protocol/BmsDbcHelper.h protocol/BmsDbcHelper.cpp
    protocol/CsvLogger.h protocol/CsvLogger.cpp
    # Model layer
    model/BmsDataModel.h model/BmsDataModel.cpp
    model/CellVoltageModel.h model/CellVoltageModel.cpp
    model/TemperatureModel.h model/TemperatureModel.cpp
    model/FaultListModel.h model/FaultListModel.cpp
    model/CommunicationMonitor.h model/CommunicationMonitor.cpp
)

# Qt policy: QTP0001 NEW — QML 资源前缀为 ':/qt/qml/' (main.cpp 加载 qrc:/qt/qml/com/bms/host/qml/main.qml)
# QTP0004 NEW: qml/components/ 等子目录的 qmldir 由 qt_add_qml_module 自动生成
qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

qt_add_qml_module(BmsHostApp
    URI "com.bms.host"
    QML_FILES
        qml/main.qml
        qml/OverviewPage.qml
        qml/CellsPage.qml
        qml/TrendsPage.qml
        qml/ControlPanel.qml
        qml/ConfigDialog.qml
        qml/components/GaugeArc.qml
        qml/components/CellBar.qml
        qml/components/FaultRibbon.qml
        qml/components/StatusIndicator.qml
        qml/DeviceStatusPage.qml
    )

target_link_libraries(BmsHostApp PRIVATE
    Qt6::Core Qt6::Quick Qt6::Qml Qt6::SerialBus Qt6::Charts Qt6::Widgets)

target_include_directories(BmsHostApp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/can
    ${CMAKE_CURRENT_SOURCE_DIR}/protocol
    ${CMAKE_CURRENT_SOURCE_DIR}/model)

set_target_properties(BmsHostApp PROPERTIES
    WIN32_EXECUTABLE TRUE
    FOLDER "BmsHostApp")
