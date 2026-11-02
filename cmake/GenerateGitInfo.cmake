# 独立脚本：在本地生成/刷新 git_info.cmake 快照
# 用法：cmake -P cmake/GenerateGitInfo.cmake
#       cmake -P cmake/GenerateGitInfo.cmake -DSOURCE_DIR=/path/to/project
cmake_minimum_required(VERSION 3.20)

if (NOT SOURCE_DIR)
    get_filename_component(SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif ()
if (NOT OUTPUT_FILE)
    set(OUTPUT_FILE "${SOURCE_DIR}/git_info.cmake")
endif ()

find_package(Git QUIET)
if (NOT GIT_FOUND)
    message(WARNING "[GenerateGitInfo] 未找到 git，未生成 ${OUTPUT_FILE}")
    return()
endif ()
if (NOT EXISTS "${SOURCE_DIR}/.git")
    message(WARNING "[GenerateGitInfo] ${SOURCE_DIR} 不是 git 仓库，未生成 ${OUTPUT_FILE}")
    return()
endif ()

set(git_cmd "${CMAKE_COMMAND}" -E env
        "GIT_CONFIG_COUNT=1"
        "GIT_CONFIG_KEY_0=safe.directory"
        "GIT_CONFIG_VALUE_0=${SOURCE_DIR}"
        "${GIT_EXECUTABLE}")

execute_process(
        COMMAND ${git_cmd} rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY "${SOURCE_DIR}"
        OUTPUT_VARIABLE branch
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE result)
if (NOT "${result}" EQUAL 0)
    set(branch "")
endif ()

execute_process(
        COMMAND ${git_cmd} rev-parse --short HEAD
        WORKING_DIRECTORY "${SOURCE_DIR}"
        OUTPUT_VARIABLE commit
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE result)
if (NOT "${result}" EQUAL 0)
    set(commit "")
endif ()

if (NOT branch OR NOT commit)
    message(WARNING "[GenerateGitInfo] 获取 git 信息失败，未生成 ${OUTPUT_FILE}")
    return()
endif ()

string(TIMESTAMP now "%Y-%m-%d %H:%M:%S")
file(WRITE "${OUTPUT_FILE}"
        "# 由 cmake/GitInfo.cmake 自动生成，请勿手动编辑\n"
        "# 用途：远程调试时远程机器没有 .git，从这里读取 git 信息\n"
        "# 生成时间: ${now}\n"
        "set(GIT_INFO_BRANCH \"${branch}\")\n"
        "set(GIT_INFO_COMMIT \"${commit}\")\n")

message(STATUS "[GenerateGitInfo] ${OUTPUT_FILE} -> ${branch} ${commit}")
