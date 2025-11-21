cmake_minimum_required(VERSION 3.20)
project(ServoSystemDemo VERSION 0.2.0)

set(CMAKE_CXX_STANDARD 14)

# 设置MSVC使用UTF-8
if(MSVC)
    # 方法1：全局设置
    add_compile_options(/utf-8)

    # 方法2：针对特定目标
    # add_executable(my_target main.cpp)
    # target_compile_options(my_target PRIVATE /utf-8)
endif()

string(TIMESTAMP COMPILE_TIME ${CMAKE_PROJECT_VERSION}_%Y%m%d_%H.%M.%S)
set(build_time ${COMPILE_TIME})
configure_file("${PROJECT_SOURCE_DIR}/version.h.in" "${PROJECT_SOURCE_DIR}/version.h")

#cmake依赖
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")
include(utility)

# https://github.com/cpm-cmake/CPM.cmake/issues/199
# 出现错误 Unknown CMake command "CPMAddPackage"
# 将CPM改为从github下载，并手动引入.cmake路径的方式
set(CPM_DOWNLOAD_VERSION 0.41.0)
set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
if (NOT (EXISTS ${CPM_DOWNLOAD_LOCATION}))
    message(STATUS "Downloading CPM.cmake v${CPM_DOWNLOAD_VERSION}")
    file(DOWNLOAD https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake ${CPM_DOWNLOAD_LOCATION})
endif ()
include(${CPM_DOWNLOAD_LOCATION})

message("ARCH ${ARCH}")


include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/3rd_libs)

find_package(gflags REQUIRED)
find_package(glog REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(fmt REQUIRED)
find_package(Poco REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(mini REQUIRED)
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(indicators REQUIRED)

# windows不支持 m pthread
if (CMAKE_HOST_SYSTEM_NAME MATCHES "Linux")
    set(LIBS ${LIBS} m pthread)
endif ()

set(LIBS ${LIBS} gflags::gflags glog::glog SQLite::SQLite3 fmt::fmt Poco::Poco yaml-cpp mini::mini CURL::libcurl openssl::openssl indicators::indicators)

##源文件列表相关
set(filtration_dirlist cmake-build-debug .idea .git conan cmake scripts test 3rd_libs)
SUBDIRLISTINCLUDE(SRC_DIRS ${CMAKE_SOURCE_DIR} 2 ${filtration_dirlist})
message(STATUS "SRC_DIRS:${SRC_DIRS}")
foreach (src_dir ${SRC_DIRS})
    aux_source_directory(${src_dir} SRC_FILES)
endforeach (src_dir)
list(REMOVE_ITEM SRC_FILES ./main.cpp)

add_executable(ServoSystemDemo main.cpp ${SRC_FILES})
target_link_libraries(ServoSystemDemo ${LIBS})
redefine_file_macro(ServoSystemDemo)

add_subdirectory(test)