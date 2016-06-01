cmake_minimum_required (VERSION 3.1)
project (sap)

set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

find_package(FFMPEG REQUIRED)
include_directories(${FFMPEG_INCLUDE_DIRS})

find_package(PkgConfig)
pkg_check_modules(GraphicsMagick GraphicsMagick++ REQUIRED)
include_directories(${GraphicsMagick_INCLUDE_DIRS})
link_directories(${GraphicsMagick_LIBRARY_DIRS})

add_subdirectory(vendor/rawr-ebooks EXCLUDE_FROM_ALL)
include_directories(vendor/rawr-ebooks vendor/rawr-ebooks/vendor/libtwittercpp/src vendor/rawr-ebooks/vendor/yaml-cpp/include)

add_executable(sap sap.cpp)
set_property(TARGET sap PROPERTY CXX_STANDARD 11)
set_property(TARGET sap PROPERTY CXX_STANDARD_REQUIRED ON)
target_link_libraries(sap rawr twitter++ ${FFMPEG_LIBRARIES} ${GraphicsMagick_LIBRARIES} yaml-cpp)
