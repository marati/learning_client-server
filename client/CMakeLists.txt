project(client)
cmake_minimum_required (VERSION 2.8 FATAL_ERROR)
set (CMAKE_IN_SOURCE_BUILD TRUE)
set (SOURCES
    main.cpp
    client.cpp
    long_actions.cpp
    file_model.cpp
    dom_item.cpp
    drop_list_widget.cpp
    mail_reader.cpp)

set (HEADERS
    dom_item.h)

set (MOC_HEADERS
    client.h
    long_actions.h
    file_model.h
    drop_list_widget.h
    mail_reader.h)

set (UIS
    client.ui)

add_definitions(-Wall -g)
set(QT_USE_QTNETWORK ON)
set(QT_USE_QTXML ON)
find_package(Qt4 REQUIRED)
include(${QT_USE_FILE})
qt4_wrap_cpp(MOC_SRCS ${MOC_HEADERS})
qt4_wrap_ui(UI_HEADERS ${UIS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
add_executable(client ${SOURCES} ${MOC_SRCS} ${HEADERS}  ${UI_HEADERS})
target_link_libraries(client ${QT_LIBRARIES})
