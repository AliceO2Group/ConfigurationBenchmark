####################################
# General project definition
####################################

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

CMAKE_MINIMUM_REQUIRED(VERSION 3.0.0 FATAL_ERROR)

### CMP0025   Compiler id for Apple Clang is now AppleClang.
### CMP0042   MACOSX_RPATH is enabled by default.

FOREACH (p
        CMP0025 # CMake 3.0
        CMP0042 # CMake 3.0
        )
    IF (POLICY ${p})
        cmake_policy(SET ${p} NEW)
    ENDIF ()
endforeach ()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake) # project specific cmake dir

project(ConfigurationBenchmark)

# Load some basic macros which are needed later on
include(O2Utils)
include(ConfigurationBenchmarkDependencies)

# Set the default build type to "RelWithDebInfo"
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo"
            CACHE
            STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel Coverage."
            FORCE)
endif(NOT CMAKE_BUILD_TYPE)

# Set the version number of your project here (format is MAJOR.MINOR.PATCHLEVEL - e.g. 1.0.0)
set(VERSION_MAJOR "1")
set(VERSION_MINOR "0")
set(VERSION_PATCH "14")
set(VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})

# C++14
IF (CMAKE_VERSION VERSION_LESS 3.1)
    include(CheckCXXCompilerFlag)
    CHECK_CXX_COMPILER_FLAG(-std=c++14 COMPILER_SUPPORTS_CXX14)
    if (COMPILER_SUPPORTS_CXX14)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
    else ()
        message(ERROR "The compiler ${CMAKE_CXX_COMPILER} has no C++14 support. Please use a different C++ compiler.")
    endif ()
ELSE ()
    set(CMAKE_CXX_STANDARD 14)  # proper way in CMake >= 3.1
ENDIF ()

# Add compiler flags for warnings and (more importantly) fPIC and debug symbols
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic -Wextra -fPIC")


####################################
# Module, library and executable definition
####################################

set(MODULE_NAME "ConfigurationBenchmark")
set(BUCKET_NAME ${BUCKET_NAME_CONFIGBENCH})

O2_SETUP(NAME ${MODULE_NAME})

set(SRCS src/Dummy.cxx)

set(LIBRARY_NAME ${MODULE_NAME})
set(BUCKET_NAME ${BUCKET_NAME_CONFIGBENCH})

# todo we repeat ourselves because the above macro dares deleting the variables we pass to it.
set(LIBRARY_NAME ${MODULE_NAME})
set(BUCKET_NAME ${BUCKET_NAME_CONFIGBENCH})

O2_GENERATE_EXECUTABLE(
        EXE_NAME configuration-benchmark
        SOURCES src/Benchmark.cxx
        BUCKET_NAME ${BUCKET_NAME}
)

add_subdirectory(doc)

# RPM generation
SET(CPACK_GENERATOR "RPM")
SET(CPACK_PACKAGE_NAME "o2-configuration-benchmark")
SET(CPACK_PACKAGE_VERSION_MAJOR ${VERSION_MAJOR})
SET(CPACK_PACKAGE_VERSION_MINOR ${VERSION_MINOR})
SET(CPACK_PACKAGE_VERSION_PATCH ${VERSION_PATCH})
SET(CPACK_RPM_PACKAGE_REQUIRES "o2-boost >= 0.0, o2-Configuration >= 0.0, o2-Monitoring >= 0.0")
SET(CPACK_RPM_PACKAGE_AUTOREQPROV " no") # Disable dependency processing, because
include(CPack)
