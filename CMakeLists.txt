cmake_minimum_required(VERSION 3.16)

project(UARTStreamParsing VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets SerialPort Test)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets SerialPort Test)

# Add QXlsx as subdirectory
add_subdirectory(third_party/QXlsx)

set(PROJECT_SOURCES
        main.cpp
        widget.cpp
        widget.h
        widget.ui
        src/config/FrameFieldDef.h
        src/config/ConfigParser.h
        src/config/ConfigParser.cpp
        src/parser/FrameParser.h
        src/parser/FrameParser.cpp
        src/parser/CrcCalculator.h
        src/parser/CrcCalculator.cpp
        src/datasource/SerialPortManager.h
        src/datasource/SerialPortManager.cpp
        src/datasource/FileDataSource.h
        src/datasource/FileDataSource.cpp
        src/export/DataExporter.h
        src/export/DataExporter.cpp
        src/parser/StreamParser.h
        src/parser/StreamParser.cpp
        src/worker/ParseWorker.h
        src/worker/ParseWorker.cpp
        resource/fonts.qrc
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(UARTStreamParsing
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    if(ANDROID)
        add_library(UARTStreamParsing SHARED
            ${PROJECT_SOURCES}
        )
    else()
        add_executable(UARTStreamParsing
            ${PROJECT_SOURCES}
            resource/app_icon.rc
        )
    endif()
endif()

target_include_directories(UARTStreamParsing PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(UARTStreamParsing PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::SerialPort
    QXlsx::QXlsx
)

enable_testing()

add_executable(UARTStreamParsingTests
    tests/tst_streamparser.cpp
    src/config/FrameFieldDef.h
    src/parser/FrameParser.h
    src/parser/FrameParser.cpp
    src/parser/CrcCalculator.h
    src/parser/CrcCalculator.cpp
    src/parser/StreamParser.h
    src/parser/StreamParser.cpp
    src/export/DataExporter.h
    src/export/DataExporter.cpp
)

target_include_directories(UARTStreamParsingTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(UARTStreamParsingTests PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Test
)

add_test(NAME UARTStreamParsingTests COMMAND UARTStreamParsingTests)

if(WIN32)
    get_target_property(QT_QMAKE_EXECUTABLE Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
    get_filename_component(CXX_COMPILER_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set_tests_properties(UARTStreamParsingTests PROPERTIES
        ENVIRONMENT "PATH=${QT_BIN_DIR}\;${CXX_COMPILER_BIN_DIR}\;$ENV{PATH}"
    )
endif()

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.UARTStreamParsing)
endif()
set_target_properties(UARTStreamParsing PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS UARTStreamParsing
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(UARTStreamParsing)
endif()
