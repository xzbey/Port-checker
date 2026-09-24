cmake_minimum_required(VERSION 3.19)
project(Port-checker LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Network Sql)

qt_standard_project_setup()
set(CMAKE_AUTORCC ON)

qt_add_executable(Port-checker
    WIN32 MACOSX_BUNDLE
    main.cpp
    gui/MainWindow.cpp gui/MainWindow.h gui/MainWindow.ui
    common/list.json
    hostchecker/HostChecker.h hostchecker/HostChecker.cpp
    common/Common.h
    common/resources.qrc
)

target_link_libraries(Port-checker
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Network
        Qt::Sql
)

target_include_directories(Port-checker
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/gui
        ${CMAKE_CURRENT_SOURCE_DIR}/hostchecker
        ${CMAKE_SOURCE_DIR}/common
)

include(GNUInstallDirs)

install(TARGETS Port-checker
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET Port-checker
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
