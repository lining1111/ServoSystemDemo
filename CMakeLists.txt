cmake_minimum_required(VERSION 3.16)
project(ServoSystemDemo VERSION 0.0.2)

set(CMAKE_CXX_STANDARD 14)

string(TIMESTAMP COMPILE_TIME ${CMAKE_PROJECT_VERSION}_%Y%m%d_%H.%M.%S)
set(build_time ${COMPILE_TIME})
configure_file("${PROJECT_SOURCE_DIR}/version.h.in" "${PROJECT_SOURCE_DIR}/version.h")

message("ARCH ${ARCH}")

include_directories(${PROJECT_SOURCE_DIR})

find_package(gflags REQUIRED)
find_package(glog REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(fmt REQUIRED)
find_package(Poco REQUIRED)
find_package(xpack REQUIRED)
find_package(mini REQUIRED)

set(LIBS m pthread gflags::gflags glog::glog SQLite::SQLite3 fmt::fmt Poco::Poco xpack::xpack mini::mini)


#cmake依赖
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")
include(utility)
include(CPM)

##源文件列表相关
set(filtration_dirlist cmake-build-debug .idea .git conan cmake scripts test)
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