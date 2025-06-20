cmake_minimum_required(VERSION 3.16)
project(DataLinkLayer VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt packages
find_package(Qt6 COMPONENTS Core Gui Widgets REQUIRED)

# Add source files
set(SOURCES
    main.cpp
    mainwindow.cpp
    datalinklayer.cpp
    frame.cpp
    crc.cpp
)

# Add header files
set(HEADERS
    mainwindow.h
    datalinklayer.h
    frame.h
    crc.h
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Include project directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link Qt libraries (modern approach)
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

# Set compiler flags for Qt
target_compile_definitions(${PROJECT_NAME} PRIVATE
    ${Qt6Core_DEFINITIONS}
    ${Qt6Gui_DEFINITIONS}
    ${Qt6Widgets_DEFINITIONS}
) 