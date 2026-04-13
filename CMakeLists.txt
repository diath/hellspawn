cmake_minimum_required(VERSION 3.21)
project(hellspawn VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(MSVC)
    add_compile_options(/W4 /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(SANITIZER_TYPE "None" CACHE STRING "Sanitizer type to enable (address, thread, memory, or None)")
    set_property(CACHE SANITIZER_TYPE PROPERTY STRINGS None address thread memory)

    if(SANITIZER_TYPE STREQUAL "None")
        message(STATUS "SANITIZER_TYPE: None (no sanitizers enabled)")
    elseif(SANITIZER_TYPE STREQUAL "address")
        message(STATUS "SANITIZER_TYPE: address (enabling AddressSanitizer + UndefinedBehaviorSanitizer)")
        add_compile_options(-fsanitize=address,undefined)
        add_link_options(-fsanitize=address,undefined)
    elseif(SANITIZER_TYPE STREQUAL "thread")
        message(STATUS "SANITIZER_TYPE: thread (enabling ThreadSanitizer + UndefinedBehaviorSanitizer)")
        add_compile_options(-fsanitize=thread,undefined)
        add_link_options(-fsanitize=thread,undefined)
    elseif(SANITIZER_TYPE STREQUAL "memory")
        message(STATUS "SANITIZER_TYPE: memory (enabling MemorySanitizer + UndefinedBehaviorSanitizer)")
        add_compile_options(-fsanitize=memory,undefined)
        add_link_options(-fsanitize=memory,undefined)
    else()
        message(FATAL_ERROR "Invalid SANITIZER_TYPE '${SANITIZER_TYPE}'. Must be one of: None, address, thread, memory")
    endif()
endif()

# ---------------------------------------------------------------------------
# CI build metadata — defaults are developer-friendly; CI overrides them via
# environment variables (CI_BUILD_ID, COMMIT_HASH) or CMake -D flags.
#
# The values are baked into src/gitinfo.h through configure_file() so that
# the About dialog can display (and link to) the exact build and commit.
# ---------------------------------------------------------------------------
set(CI_BUILD_ID "0" CACHE STRING "CI build run ID (used in the About dialog link)")
set(COMMIT_HASH "Developer Version" CACHE STRING "Git commit hash (used in the About dialog link)")

# Environment variables take precedence — this lets CI simply export the
# variables before invoking the build script without modifying the script.
if(DEFINED ENV{CI_BUILD_ID})
    set(CI_BUILD_ID "$ENV{CI_BUILD_ID}")
endif()
if(DEFINED ENV{COMMIT_HASH})
    set(COMMIT_HASH "$ENV{COMMIT_HASH}")
endif()

message(STATUS "CI Build ID:  ${CI_BUILD_ID}")
message(STATUS "Commit Hash:  ${COMMIT_HASH}")

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gitinfo.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/gitinfo.h"
    @ONLY
)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

find_package(fmt CONFIG REQUIRED)
find_package(pugixml CONFIG REQUIRED)

set(UTIL_SOURCES
    src/util/Logger.h
    src/util/Logger.cpp
    src/util/BinaryReader.h
    src/util/BinaryReader.cpp
    src/util/BinaryWriter.h
    src/util/BinaryWriter.cpp
    src/util/StringHelper.h
    src/util/Timing.h
)

set(CORE_SOURCES
    src/core/SpriteData.h
    src/core/ThingData.h
    src/core/SpriteFile.h
    src/core/SpriteFile.cpp
    src/core/DatFile.h
    src/core/DatFile.cpp
    src/core/OtbFile.h
    src/core/OtbFile.cpp
    src/core/ItemsXml.h
    src/core/ItemsXml.cpp
)

set(WIDGET_SOURCES
    src/widgets/SpriteGrid.h
    src/widgets/SpriteGrid.cpp
    src/widgets/SpritePreview.h
    src/widgets/SpritePreview.cpp
    src/widgets/AnimationPreview.h
    src/widgets/AnimationPreview.cpp
)

set(EDITOR_SOURCES
    src/editor/MainWindow.h
    src/editor/MainWindow.cpp
    src/editor/EditorTab.h
    src/editor/EditorTab.cpp
    src/editor/LoadWidget.h
    src/editor/LoadWidget.cpp
    src/editor/EditorContent.h
    src/editor/EditorContent.cpp
    src/editor/SpriteEditor.h
    src/editor/SpriteEditor.cpp
    src/editor/ThingEditor.h
    src/editor/ThingEditor.cpp
    src/editor/ThingIconProvider.h
    src/editor/ThingIconProvider.cpp
    src/editor/ThingPropertyEditor.h
    src/editor/ThingPropertyEditor.cpp
)

# ---------------------------------------------------------------------------
# Qt resource file — embeds hellspawn.png so it is available at runtime via
# the Qt resource system (e.g. ":/hellspawn.png").  CMAKE_AUTORCC handles
# compilation automatically.
# ---------------------------------------------------------------------------
set(RESOURCE_FILES
    resources.qrc
)

# ---------------------------------------------------------------------------
# Windows executable icon — if hellspawn.ico exists next to the source tree
# we compile hellspawn.rc which embeds the icon into the .exe so that
# Windows Explorer shows it.  CI generates the .ico from the PNG before
# the build step; local developers can convert it manually or simply skip
# this (the Qt window icon from the resource file still works).
# ---------------------------------------------------------------------------
set(WIN32_RESOURCE_FILES "")
if(WIN32)
    set(_ico_path "${CMAKE_CURRENT_SOURCE_DIR}/hellspawn.ico")
    if(EXISTS "${_ico_path}")
        set(WIN32_RESOURCE_FILES hellspawn.rc)
        message(STATUS "Windows icon: embedding hellspawn.ico into executable")
    else()
        message(STATUS "Windows icon: hellspawn.ico not found — .exe will not have an embedded icon")
    endif()
endif()

add_executable(hellspawn
    src/main.cpp
    ${UTIL_SOURCES}
    ${CORE_SOURCES}
    ${WIDGET_SOURCES}
    ${EDITOR_SOURCES}
    ${RESOURCE_FILES}
    ${WIN32_RESOURCE_FILES}
)

target_include_directories(hellspawn PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_link_libraries(hellspawn PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    fmt::fmt-header-only
    pugixml::pugixml
)

if(WIN32)
    set_target_properties(hellspawn PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()
