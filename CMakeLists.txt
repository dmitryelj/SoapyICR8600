########################################################################
# Build Soapy SDR support module for ICR8600
########################################################################
cmake_minimum_required(VERSION 2.8.7)
project(SoapyICR8600 CXX)

find_package(SoapySDR "0.4.0" NO_MODULE REQUIRED)
if (NOT SoapySDR_FOUND)
    message(FATAL_ERROR "Soapy SDR development files not found...")
endif ()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})

#enable c++11 features
if(CMAKE_COMPILER_IS_GNUCXX)

    #C++11 is a required language feature for this project
    include(CheckCXXCompilerFlag)
    CHECK_CXX_COMPILER_FLAG("-std=c++11" HAS_STD_CXX11)
    if(HAS_STD_CXX11)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
    else(HAS_STD_CXX11)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
    endif()

    #disable warnings for unused parameters
    add_definitions(-Wno-unused-parameter)

endif(CMAKE_COMPILER_IS_GNUCXX)

set(OTHER_LIBS "" CACHE STRING "Other libraries")
if (APPLE)
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wc++11-extensions -I/opt/local/include")
   list(APPEND OTHER_LIBS -lusb-1.0)
endif(APPLE)

if (LINUX)
    list(APPEND OTHER_LIBS -lusb-1.0)
endif(LINUX)

SOAPY_SDR_MODULE_UTIL(
    TARGET icr8600Support
    SOURCES
        SoapyICR8600.hpp
        Registation.cpp
        Settings.cpp
        Streaming.cpp
		WinUSBDevice.cpp
		WinUSBDevice.h
    LIBRARIES
        ${OTHER_LIBS}
)
