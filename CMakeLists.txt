cmake_minimum_required(VERSION 3.1.0 FATAL_ERROR)
# CMP0000: Call the cmake_minimum_required() command at the beginning of the top-level
# CMakeLists.txt file even before calling the project() command.
# The cmake_minimum_required(VERSION) command implicitly invokes the cmake_policy(VERSION)
# command to specify that the current project code is written for the given range of CMake
# versions.
project(pavucontrol-qt)

option(UPDATE_TRANSLATIONS "Update source translation translations/*.ts files" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

include(GNUInstallDirs)

option(ENABLE_SANITIZERS "Enable runtime sanitizing (for development)")
if (ENABLE_SANITIZERS)
    message("Enabling asan and ubsan")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined")
endif()

# Minimum Versions
set(QT_MINIMUM_VERSION "5.10.0")

find_package(Qt5Widgets ${QT_MINIMUM_VERSION} REQUIRED)
find_package(Qt5LinguistTools ${QT_MINIMUM_VERSION} REQUIRED)

set(PAVUCONTROLQT_MAJOR_VERSION 0)
set(PAVUCONTROLQT_MINOR_VERSION 15)
set(PAVUCONTROLQT_PATCH_VERSION 0)
set(PAVUCONTROLQT_VERSION ${PAVUCONTROLQT_MAJOR_VERSION}.${PAVUCONTROLQT_MINOR_VERSION}.${PAVUCONTROLQT_PATCH_VERSION})
add_definitions("-DPAVUCONTROLQT_VERSION=\"${PAVUCONTROLQT_VERSION}\"")

find_package(PkgConfig REQUIRED)
pkg_check_modules(
    PULSE REQUIRED
    libpulse>=5.0
)

add_subdirectory(src)
