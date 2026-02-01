cmake_minimum_required(VERSION 3.16)
project(LuminaTask VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt6 components
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

# Enable Qt6 automoc
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Add executable
add_executable(LuminaTask
    src/main.cpp
    src/processmanager.cpp
    src/mainwindow.cpp
    include/processmanager.h
    include/mainwindow.h
)

# Include directories
target_include_directories(LuminaTask PRIVATE include)

# Link Qt6 libraries
target_link_libraries(LuminaTask
    Qt6::Core
    Qt6::Widgets
)

# Compiler flags for optimization
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(LuminaTask PRIVATE -O3 -march=native)
endif()

# Install target
install(TARGETS LuminaTask
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)