cmake_minimum_required(VERSION 3.18.1)

# Configure build library name.
project("dlib_jni")
set(TARGET_NAME dlib_jni)


include(FetchContent)

# Dlib
# Downloads Dlib direct from github
FetchContent_Declare(dlib
        GIT_REPOSITORY https://github.com/davisking/dlib.git
        GIT_TAG v19.22
        GIT_SHALLOW ON
        )
FetchContent_MakeAvailable(dlib)

# Configure install folder for:
# 1) Finding the dependent libraries.
# 2) Copy out the generated files for others to use.
set(INSTALL_DIR ${CMAKE_SOURCE_DIR}/../../../../lib-distribution/)

# Configure import libs.
set(LIB_DIR ${CMAKE_SOURCE_DIR}/../cppLibs)

# Link to libprotobuf-lite.so.
add_library(protobuf SHARED IMPORTED)
set_target_properties(protobuf PROPERTIES IMPORTED_LOCATION
    ${LIB_DIR}/protobuf/lib/${ANDROID_ABI}/libprotobuf-lite-3.2.0.so)

# Build project shared lib
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions -std=c++11")
add_library(${TARGET_NAME} SHARED
            jni.cpp
            profiler.cpp
            dlib-face-landmarks-detector-jni.cpp
            include/my/dlib/data/messages.pb.cc)
target_include_directories(${TARGET_NAME} PRIVATE
                           ${CMAKE_SOURCE_DIR}/include
                           ${LIB_DIR}/protobuf/include
                           )
target_link_libraries(${TARGET_NAME}
                      dlib
                      protobuf
                      android
                      jnigraphics
                      log)

# Make install directory.
file(MAKE_DIRECTORY ${INSTALL_DIR}/${TARGET_NAME})
file(MAKE_DIRECTORY ${INSTALL_DIR}/${TARGET_NAME}/include)
file(MAKE_DIRECTORY ${INSTALL_DIR}/${TARGET_NAME}/lib)

# Copy generated protobuf message header file.
file(COPY "${CMAKE_SOURCE_DIR}/include/my/dlib/data/messages.pb.h"
     DESTINATION "${INSTALL_DIR}/${TARGET_NAME}/include/my/dlib/data/")

# Copy out the project binary.
add_custom_command(TARGET ${TARGET_NAME} POST_BUILD

#                   # Copy pre-built *.so files.
#                   COMMAND "${CMAKE_COMMAND}" -E
#                   copy_directory "${LIB_DIR}/dlib/lib/${ANDROID_ABI}"
#                   "${INSTALL_DIR}/${TARGET_NAME}/lib/${ANDROID_ABI}"

                   # Copy generated *.so files.
                   COMMAND "${CMAKE_COMMAND}" -E
                   copy_directory "${CMAKE_SOURCE_DIR}/../../../build/intermediates/cmake/${CMAKE_BUILD_TYPE}/obj/${ANDROID_ABI}"
                   "${INSTALL_DIR}/${TARGET_NAME}/lib/${ANDROID_ABI}"

                   COMMENT "Copying ${TARGET_NAME} shared library to output directory")
