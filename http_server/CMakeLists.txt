project(http_server)
cmake_minimum_required (VERSION 2.8 FATAL_ERROR)
set (CMAKE_IN_SOURCE_BUILD TRUE)
set (SOURCES
    main.cpp
    server.cpp
    dir_scanner.cpp
    tcp_server.cpp
    http_server.cpp
    long_actions.cpp
    logger.cpp)

set (MOC_HEADERS
    server.h
    tcp_server.h
    dir_scanner.h
    http_server.h
    long_actions.h)

set (UIS)

set (HEADERS
    logger.h)

add_definitions(-Wall -g)
set(QT_USE_QTNETWORK ON)
set(QT_USE_QTXML ON)
find_package(Qt4 REQUIRED)
include(${QT_USE_FILE})
qt4_wrap_cpp(MOC_SRCS ${MOC_HEADERS})
qt4_wrap_ui(UI_HEADERS ${UIS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
add_executable(http_server ${SOURCES} ${HEADERS} ${MOC_SRCS} ${UI_HEADERS})
target_link_libraries(http_server ${QT_LIBRARIES})
