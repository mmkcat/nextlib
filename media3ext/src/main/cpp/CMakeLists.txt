# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

cmake_minimum_required(VERSION 3.22.1)
project("media3ext")
set(CMAKE_CXX_STANDARD 17)

set(ffmpeg_dir ${CMAKE_SOURCE_DIR}/../../../../ffmpeg/output)
set(ffmpeg_libs ${ffmpeg_dir}/lib/${ANDROID_ABI})
include_directories(${ffmpeg_dir}/include/${ANDROID_ABI})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(ffmpeg_libs_names avutil avcodec avformat swresample swscale ass)

foreach(ffmpeg_lib_name ${ffmpeg_libs_names})
    add_library(${ffmpeg_lib_name} SHARED IMPORTED)
    set_target_properties(${ffmpeg_lib_name} PROPERTIES
        IMPORTED_LOCATION ${ffmpeg_libs}/lib${ffmpeg_lib_name}.so)
endforeach()

add_library(${CMAKE_PROJECT_NAME} SHARED
    ffmain.cpp
    ffcommon.cpp
    ffaudio.cpp
    ffvideo.cpp
    fftext.cpp
    ffextractor.cpp
    openjoc_jni.cpp)

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,max-page-size=16384")

target_link_libraries(${CMAKE_PROJECT_NAME}
    log
    android
    jnigraphics
    z
    dl
    ${ffmpeg_libs_names})
