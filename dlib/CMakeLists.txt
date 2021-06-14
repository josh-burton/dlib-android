cmake_minimum_required(VERSION 3.18.1)

project("dlib_jni")

include(FetchContent)
FetchContent_Declare(dlib
        GIT_REPOSITORY https://github.com/davisking/dlib.git
        GIT_TAG v19.22
        )
FetchContent_MakeAvailable(dlib)

# generated messages path
string(TOLOWER ${CMAKE_BUILD_TYPE} BUILD_TYPE_DIR)
set(MESSAGES_GEN_DIR build/generated/source/proto/${BUILD_TYPE_DIR}/cpp)

# Sets lib_src_DIR to the path of the target CMake project.
set(PROTOBUF_SRC_DIR libs/protobuf/cmake)

# Sets lib_build_DIR to the path of the desired output directory.
set( PROTOBUF_BUILD_DIR src/main/cpp/build/generated/source/protobuf)
file(MAKE_DIRECTORY ${PROTOBUF_BUILD_DIR})

# Adds the CMakeLists.txt file located in the specified directory
# as a build dependency.
add_subdirectory( # Specifies the directory of the CMakeLists.txt file.
        ${PROTOBUF_SRC_DIR}

        # Specifies the directory for the build outputs.
        ${PROTOBUF_BUILD_DIR} )

# Adds the output of the additional CMake build as a prebuilt static
# library and names it libprotobuf.
include_directories(${PROTOBUF_SRC_DIR}/../src)

include_directories(${MESSAGES_GEN_DIR})

add_library( # Sets the name of the library.
        dlib_jni
        # Sets the library as a shared library.
        SHARED

        src/main/cpp/jni.cpp
        src/main/cpp/profiler.cpp
        src/main/cpp/dlib-face-landmarks-detector-jni.cpp
        build/generated/source/proto/${BUILD_TYPE_DIR}/cpp/messages.pb.cc
        )

find_library( # Sets the name of the path variable.
        log-lib
        # Specifies the name of the NDK library that
        # you want CMake to locate.
        log)

target_link_libraries( # Specifies the target library.
        dlib_jni
        dlib
        android
        libprotobuf-lite
        jnigraphics
        # Links the target library to the log library
        # included in the NDK.
        ${log-lib})

