cmake_minimum_required(VERSION 3.16)

project(Tokri VERSION 2026.09.19 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# main.cpp includes the generated ui_tokriwindow.h; the .ui file lives with the
# window implementation in src/windows.
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/src/windows)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

string(REGEX REPLACE "^([0-9]+)\\.([0-9]+)\\.([0-9]+).*$" "\\1-\\2-\\3"
       TOKRI_RELEASE_DATE "${PROJECT_VERSION}")
if(NOT TOKRI_RELEASE_DATE MATCHES "^[0-9]+-[0-9]+-[0-9]+$")
    string(TIMESTAMP TOKRI_RELEASE_DATE "%Y-%m-%d")
endif()

configure_file(
    resources/com.ysuraj.Tokri.metainfo.xml.in
    "${CMAKE_CURRENT_BINARY_DIR}/com.ysuraj.Tokri.metainfo.xml" @ONLY)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Network Widgets Concurrent)
find_package(Qt6 OPTIONAL_COMPONENTS DBus)

# ---------------------------------------------------------------------------
# Sources, grouped by area. Platform variants live next to each other under
# src/platform; only the variant for the current OS is compiled.
# ---------------------------------------------------------------------------

set(TOKRI_SOURCES
    src/main.cpp

    # Shared infrastructure: logging and well-known paths/names.
    src/common/loghelpers.h
    src/common/standardnames.h
    src/common/standardpaths.h
    src/common/standardpaths.cpp

    # Turning dropped data into files in the basket directory.
    src/drop/drophandler.h
    src/drop/drophandler.cpp
    src/drop/filepathprovider.h
    src/drop/filepathprovider.cpp
    src/drop/textfile.h
    src/drop/textfile.cpp

    # Item models: filesystem -> thumbnails -> sort/filter.
    src/models/dropawarefilesystemmodel.h
    src/models/dropawarefilesystemmodel.cpp
    src/models/thumbnailproxymodel.h
    src/models/thumbnailproxymodel.cpp
    src/models/sortfilterproxy.h
    src/models/sortfilterproxy.cpp

    # List view, item delegate and shared tile geometry.
    src/views/nointernaldraglistview.h
    src/views/nointernaldraglistview.cpp
    src/views/thumbnaildelegate.h
    src/views/thumbnaildelegate.cpp
    src/views/thumbnaillayout.h

    # Application windows and dialogs.
    src/windows/tokriwindow.h
    src/windows/tokriwindow.cpp
    src/windows/tokriwindow.ui
    src/windows/aboutwindow.h
    src/windows/aboutwindow.cpp
    src/windows/helpwindow.h
    src/windows/helpwindow.cpp
    src/windows/logwindow.h
    src/windows/logwindow.cpp

    # Palette shared by widgets, tray menu and windows.
    src/theme/themeprovider.h
    src/theme/themeprovider.cpp

    # Platform shims that keep per-OS branches behind one function.
    src/platform/windowicons.h
    src/platform/windowicons.cpp
    src/platform/filereveal.h
    src/platform/filereveal.cpp

    # Shared shake gesture detector.
    src/platform/shake/shakedetector.h
    src/platform/shake/shakedetector.cpp

    resources/icons.qrc
)

if(APPLE)
    list(APPEND TOKRI_SOURCES
        src/platform/shake/macosdragshakedetector.h
        src/platform/shake/macosdragshakedetector.cpp
    )
elseif(WIN32)
    list(APPEND TOKRI_SOURCES
        src/platform/shake/windowsdragshakedetector.h
        src/platform/shake/windowsdragshakedetector.cpp
    )
elseif(UNIX)
    list(APPEND TOKRI_SOURCES
        src/platform/shake/linuxdragshakedetector.h
        src/platform/shake/linuxdragshakedetector.cpp
    )
endif()

qt_add_executable(Tokri
    MANUAL_FINALIZATION
    ${TOKRI_SOURCES}
)

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

# Windows exe icon
if(WIN32)
    target_sources(Tokri PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/tokri.rc
    )
endif()

set_target_properties(Tokri PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.ysuraj.Tokri"
    MACOSX_BUNDLE_BUNDLE_NAME "Tokri"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS Tokri
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

target_link_libraries(Tokri PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Widgets
    Qt6::Concurrent
)

target_include_directories(Tokri PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/resources
)

target_compile_definitions(Tokri PRIVATE TOKRI_VERSION="${PROJECT_VERSION}")

if(APPLE)
    target_link_libraries(Tokri PRIVATE "-framework CoreGraphics")
endif()

if(WIN32)
    target_link_libraries(Tokri PRIVATE user32)
endif()

if(UNIX AND NOT APPLE AND TARGET Qt6::DBus)
    target_link_libraries(Tokri PRIVATE Qt6::DBus)
    target_compile_definitions(Tokri PRIVATE TOKRI_HAVE_DBUS)
endif()

if(UNIX AND NOT APPLE)
    install(
      FILES resources/com.ysuraj.Tokri.png
      DESTINATION share/icons/hicolor/256x256/apps
    )
    install(
      FILES resources/com.ysuraj.Tokri.desktop
      DESTINATION share/applications
    )
    install(
      FILES "${CMAKE_CURRENT_BINARY_DIR}/com.ysuraj.Tokri.metainfo.xml"
      DESTINATION share/metainfo
    )
endif()

qt_finalize_executable(Tokri)
