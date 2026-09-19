cmake_minimum_required(VERSION 3.16)

# include(FetchContent)

project(Tokri VERSION 2026.09.19 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Gui)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Concurrent)

set(PROJECT_SOURCES
        src/main.cpp
        src/tokriwindow.cpp
        src/tokriwindow.h
        src/tokriwindow.ui
        src/windowicons.h
        src/windowicons.cpp
)

set(LINUX_SOURCES "")
if (UNIX AND NOT APPLE)
    list(APPEND LINUX_SOURCES
        src/linuxdragshakedetector.h
        src/linuxdragshakedetector.cpp
    )
endif()

set(WINDOWS_SOURCES "")
if (WIN32)
    list(APPEND WINDOWS_SOURCES
        src/windowsdragshakedetector.h
        src/windowsdragshakedetector.cpp
    )
endif()

set(MACOS_SOURCES "")
if (APPLE)
    list(APPEND MACOS_SOURCES
        src/macosdragshakedetector.h
        src/macosdragshakedetector.cpp
    )
endif()

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(Tokri
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}

        src/dropawarefilesystemmodel.h src/dropawarefilesystemmodel.cpp
        src/drophandler.h src/drophandler.cpp
        src/textfile.h src/textfile.cpp
        src/filepathprovider.h src/filepathprovider.cpp
        src/loghelpers.h
        src/sortfilterproxy.h src/sortfilterproxy.cpp
        src/thumbnailproxymodel.h src/thumbnailproxymodel.cpp
        src/thumbnaildelegate.h src/thumbnaildelegate.cpp
        src/thumbnaillayout.h
        src/nointernaldraglistview.h src/nointernaldraglistview.cpp
        src/standardnames.h
        src/shakedetector.h src/shakedetector.cpp

        src/themeprovider.h src/themeprovider.cpp

        src/standardpaths.h src/standardpaths.cpp

        ${LINUX_SOURCES}
        ${WINDOWS_SOURCES}
        ${MACOS_SOURCES}

        resources/icons.qrc
        src/logwindow.h src/logwindow.cpp
        src/helpwindow.h src/helpwindow.cpp
        src/aboutwindow.h src/aboutwindow.cpp
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET Tokri APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(Tokri SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(Tokri
            ${PROJECT_SOURCES}
        )
    endif()
endif()

# macOS app icon
if(APPLE)
    set(MACOSX_BUNDLE_ICON_FILE Tokri.icns)

    target_sources(Tokri PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/Tokri.icns
    )

    set_source_files_properties(
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/Tokri.icns
        PROPERTIES MACOSX_PACKAGE_LOCATION Resources
    )
endif()

set_target_properties(Tokri PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER "net.surajyadav.Tokri"
    MACOSX_BUNDLE_BUNDLE_NAME "Tokri"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING
        "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS Tokri
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(Tokri)
endif()

# FetchContent_Declare(rapidfuzz
#   GIT_REPOSITORY https://github.com/rapidfuzz/rapidfuzz-cpp.git
#   GIT_TAG main)
# FetchContent_MakeAvailable(rapidfuzz)
# target_link_libraries(Tokri PRIVATE rapidfuzz::rapidfuzz)

target_link_libraries(Tokri PRIVATE Qt${QT_VERSION_MAJOR}::Core)
target_link_libraries(Tokri PRIVATE Qt${QT_VERSION_MAJOR}::Gui)
target_link_libraries(Tokri PRIVATE Qt${QT_VERSION_MAJOR}::Network)
target_link_libraries(Tokri PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
target_link_libraries(Tokri PRIVATE Qt${QT_VERSION_MAJOR}::Concurrent)

target_include_directories(Tokri PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(Tokri PRIVATE TOKRI_VERSION="${PROJECT_VERSION}")
# include_directories(${CMAKE_CURRENT_SOURCE_DIR})

if(APPLE)
    target_link_libraries(Tokri PRIVATE
        "-framework CoreGraphics"
    )
endif()

if(WIN32)
    target_link_libraries(Tokri PRIVATE
        user32
    )
endif()

if(UNIX AND NOT APPLE)
        install(
          FILES resources/net.surajyadav.Tokri.png
          DESTINATION share/icons/hicolor/256x256/apps
        )
        install(
          FILES resources/net.surajyadav.Tokri.desktop
          DESTINATION share/applications
        )
endif()
