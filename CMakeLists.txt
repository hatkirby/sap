cmake_minimum_required (VERSION 3.1)
project (sap)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

find_package(FFMPEG REQUIRED)

find_package(PkgConfig)
pkg_check_modules(yaml-cpp yaml-cpp REQUIRED)
pkg_check_modules(GraphicsMagick GraphicsMagick++ REQUIRED)

add_subdirectory(vendor/rawr-ebooks EXCLUDE_FROM_ALL)
add_subdirectory(vendor/mastodonpp)

include_directories(
  ${FFMPEG_INCLUDE_DIRS}
  vendor/rawr-ebooks
  vendor/mastodonpp/include
  vendor/json
  ${yaml-cpp_INCLUDE_DIRS}
  ${GraphicsMagick_INCLUDE_DIRS})

link_directories(
  ${yaml-cpp_LIBRARY_DIRS}
  ${GraphicsMagick_LIBRARY_DIRS})

add_executable(sap sap.cpp director.cpp designer.cpp main.cpp)
set_property(TARGET sap PROPERTY CXX_STANDARD 17)
set_property(TARGET sap PROPERTY CXX_STANDARD_REQUIRED ON)

target_link_libraries(sap
  rawr
  mastodonpp
  ${FFMPEG_LIBRARIES}
  ${GraphicsMagick_LIBRARIES}
  ${yaml-cpp_LIBRARIES})
