############################################################
# dispatcher - Dispatch Queue Library
############################################################

cmake_minimum_required(VERSION 3.16)

project(dispatcher
    VERSION 1.0.0
    DESCRIPTION "Dispatch Queue Library"
    LANGUAGES CXX
)

# C++17 is required
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(dispatcher_BUILD_SHARED "Build shared library" OFF)
option(dispatcher_BUILD_EXAMPLES "Build examples" ON)

# Source files
set(dispatcher_HEADERS
    include/dispatcher/Types.h
    include/dispatcher/IDispatchQueue.h
    include/dispatcher/IQueueListener.h
    include/dispatcher/TaskQueue.h
    include/dispatcher/ThreadedDispatchQueue.h
    include/dispatcher/DispatchQueue.h
    include/dispatcher/ThreadPoolDispatchQueue.h
)

set(dispatcher_SOURCES
    src/TaskQueue.cpp
    src/ThreadedDispatchQueue.cpp
    src/DispatchQueue.cpp
    src/ThreadPoolDispatchQueue.cpp
)

# Create library
if(dispatcher_BUILD_SHARED)
    add_library(dispatcher SHARED ${dispatcher_SOURCES} ${dispatcher_HEADERS})
    target_compile_definitions(dispatcher PRIVATE dispatcher_EXPORTS)
else()
    add_library(dispatcher STATIC ${dispatcher_SOURCES} ${dispatcher_HEADERS})
    target_compile_definitions(dispatcher PUBLIC dispatcher_STATIC)
endif()

# Include directories
target_include_directories(dispatcher
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Compiler warnings
if(MSVC)
    target_compile_options(dispatcher PRIVATE /W4)
else()
    target_compile_options(dispatcher PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Alias for modern CMake
add_library(dispatcher::dispatcher ALIAS dispatcher)

# Examples
if(dispatcher_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Install rules
include(GNUInstallDirs)

install(TARGETS dispatcher
    EXPORT dispatcherTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/dispatcher
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT dispatcherTargets
    FILE dispatcherTargets.cmake
    NAMESPACE dispatcher::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dispatcher
)

# Generate Config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/dispatcherConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/dispatcherConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/dispatcherConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dispatcher
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/dispatcherConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/dispatcherConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/dispatcher
)

# Summary
message(STATUS "")
message(STATUS "dispatcher ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Shared library: ${dispatcher_BUILD_SHARED}")
message(STATUS "  Examples: ${dispatcher_BUILD_EXAMPLES}")
message(STATUS "")