# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.
cmake_minimum_required(VERSION 3.18.1)

# Declares and names the project.

project("dlib_jni")

include(FetchContent)

# Dlib
FetchContent_Declare(dlib
        GIT_REPOSITORY https://github.com/davisking/dlib.git
        GIT_TAG v19.22
        )
FetchContent_MakeAvailable(dlib)

#include(FindProtobuf)
#find_package(Protobuf REQUIRED)
#include_directories(${PROTOBUF_INCLUDE_DIR})

# Protocol Buffers
#
FetchContent_Declare(
        protobuf
        GIT_REPOSITORY https://github.com/protocolbuffers/protobuf.git
        GIT_TAG v3.17.0
        SOURCE_SUBDIR cmake
)
FetchContent_MakeAvailable(protobuf)
#include_directories(${PROTOBUF_INCLUDE_DIRS})
#include_directories(${Protobuf_INCLUDE_DIR})

find_package(protobuf REQUIRED)
include_directories(
        ${Protobuf_INCLUDE_DIRS}
)


#include_directories(${CMAKE_CURRENT_BINARY_DIR})
#find_package(protobuf 3.17.0 REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions -std=c++11")

#message(STATUS "Using Protocol Buffers ${Protobuf_VERSION}")

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.
add_library( # Sets the name of the library.
        dlib_jni
        # Sets the library as a shared library.
        SHARED

        jni.cpp
        profiler.cpp
        dlib-face-landmarks-detector-jni.cpp
        )

# Searches for a specified prebuilt library and stores the path as a
# variable. Because CMake includes system libraries in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.

find_library( # Sets the name of the path variable.
        log-lib
        # Specifies the name of the NDK library that
        # you want CMake to locate.
        log)


# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

include_directories(${PROTOBUF_INCLUDE_DIRS})
target_include_directories(dlib_jni PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${Protobuf_INCLUDE_DIRS}
        ${PROTOBUF_INCLUDE_DIRS}
        )
#        ${LIB_DIR}/protobuf/include
#        ${LIB_DIR}/dlib/include)

target_link_libraries( # Specifies the target library.
        dlib_jni
        dlib
        protobuf
        android
        jnigraphics
        # Links the target library to the log library
        # included in the NDK.
        ${log-lib})

