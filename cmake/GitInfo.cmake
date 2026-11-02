# ============================================================================
# Git 信息获取（支持 CLion 远程调试）
#
# 背景：远程调试时 cmake 在远程机器上执行，远程机器通常没有 .git 目录、
#       也没有 git 可执行文件，拿不到分支/commit。
# 方案：本地 configure 时把 git 信息写入工程内快照文件 git_info.cmake，
#       该文件随 CLion 自动上传同步到远程；远程 configure 时优先读快照。
#
# 取值优先级：
#   1) CMake 缓存变量 -DGIT_BRANCH=xxx / -DGIT_COMMIT=xxx（或同名环境变量）
#   2) 快照文件 ${GIT_INFO_FILE}（默认 ${CMAKE_SOURCE_DIR}/git_info.cmake）
#   3) 本机 git 命令（成功后自动刷新快照文件）
#   4) 兜底 "unknown"
#
# 常用选项：
#   -DGIT_INFO_FILE=<路径>   指定快照文件位置
#   -DGIT_INFO_REFRESH=ON    忽略快照，强制用 git 重新获取并覆盖快照
# ============================================================================

set(GIT_INFO_FILE "${CMAKE_SOURCE_DIR}/git_info.cmake" CACHE FILEPATH
        "git 信息快照文件（本地生成，随上传同步到远程）")
set(GIT_INFO_REFRESH OFF CACHE BOOL "忽略快照，强制用 git 命令重新获取")
set(GIT_BRANCH "" CACHE STRING "手动指定 Git 分支名（留空则自动获取）")
set(GIT_COMMIT "" CACHE STRING "手动指定 Git commit hash（留空则自动获取）")

# 通过 git 命令获取，取不到时返回空字符串
function(_git_info_from_git out_branch out_commit)
    set(branch "")
    set(commit "")

    find_package(Git QUIET)
    if (GIT_FOUND AND GIT_EXECUTABLE AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
        # git 2.35.2+ 对不属于当前用户的仓库会报 "detected dubious ownership"，
        # 远程/挂载的源码目录很容易踩到，这里临时把它标记为安全目录
        set(git_cmd "${CMAKE_COMMAND}" -E env
                "GIT_CONFIG_COUNT=1"
                "GIT_CONFIG_KEY_0=safe.directory"
                "GIT_CONFIG_VALUE_0=${CMAKE_SOURCE_DIR}"
                "${GIT_EXECUTABLE}")

        execute_process(
                COMMAND ${git_cmd} rev-parse --abbrev-ref HEAD
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                OUTPUT_VARIABLE branch
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
                RESULT_VARIABLE result)
        # 命令失败时清空，避免把 git 的报错文本写进版本号
        if (NOT "${result}" EQUAL 0)
            set(branch "")
        endif ()

        execute_process(
                COMMAND ${git_cmd} rev-parse --short HEAD
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                OUTPUT_VARIABLE commit
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
                RESULT_VARIABLE result)
        if (NOT "${result}" EQUAL 0)
            set(commit "")
        endif ()
    endif ()

    set(${out_branch} "${branch}" PARENT_SCOPE)
    set(${out_commit} "${commit}" PARENT_SCOPE)
endfunction()

# 读取快照文件
function(_git_info_from_snapshot out_branch out_commit)
    set(branch "")
    set(commit "")
    if (EXISTS "${GIT_INFO_FILE}")
        set(GIT_INFO_BRANCH "")
        set(GIT_INFO_COMMIT "")
        include("${GIT_INFO_FILE}")
        set(branch "${GIT_INFO_BRANCH}")
        set(commit "${GIT_INFO_COMMIT}")
    endif ()
    set(${out_branch} "${branch}" PARENT_SCOPE)
    set(${out_commit} "${commit}" PARENT_SCOPE)
endfunction()

# 写入快照文件
function(_git_info_write_snapshot branch commit)
    string(TIMESTAMP now "%Y-%m-%d %H:%M:%S")
    file(WRITE "${GIT_INFO_FILE}"
            "# 由 cmake/GitInfo.cmake 自动生成，请勿手动编辑\n"
            "# 用途：远程调试时远程机器没有 .git，从这里读取 git 信息\n"
            "# 生成时间: ${now}\n"
            "set(GIT_INFO_BRANCH \"${branch}\")\n"
            "set(GIT_INFO_COMMIT \"${commit}\")\n")
endfunction()

# 对外接口：get_git_info(<branch_var> <commit_var>)
function(get_git_info out_branch out_commit)
    # 输出变量可能与下面的输入同名，先移除普通变量，只保留用户 -D 传入的缓存值
    unset(GIT_BRANCH)
    unset(GIT_COMMIT)

    set(branch "")
    set(commit "")
    set(source "")

    # 1) 手动指定：缓存变量 / 同名环境变量
    if (NOT GIT_BRANCH AND DEFINED ENV{GIT_BRANCH})
        set(GIT_BRANCH "$ENV{GIT_BRANCH}")
    endif ()
    if (NOT GIT_COMMIT AND DEFINED ENV{GIT_COMMIT})
        set(GIT_COMMIT "$ENV{GIT_COMMIT}")
    endif ()
    if (GIT_BRANCH OR GIT_COMMIT)
        set(branch "${GIT_BRANCH}")
        set(commit "${GIT_COMMIT}")
        set(source "手动指定")
    endif ()

    # 2) 快照文件（远程机器上主要走这一条）
    if ((NOT branch OR NOT commit) AND NOT GIT_INFO_REFRESH AND EXISTS "${GIT_INFO_FILE}")
        _git_info_from_snapshot(snap_branch snap_commit)
        if (snap_branch OR snap_commit)
            if (NOT branch)
                set(branch "${snap_branch}")
            endif ()
            if (NOT commit)
                set(commit "${snap_commit}")
            endif ()
            set(source "快照文件 ${GIT_INFO_FILE}")
        endif ()
    endif ()

    # 3) 本机 git 命令，成功后刷新快照
    if (NOT branch OR NOT commit OR GIT_INFO_REFRESH)
        _git_info_from_git(git_branch_ git_commit_)
        if (git_branch_ OR git_commit_)
            if (NOT branch OR GIT_INFO_REFRESH)
                set(branch "${git_branch_}")
            endif ()
            if (NOT commit OR GIT_INFO_REFRESH)
                set(commit "${git_commit_}")
            endif ()
            set(source "git 命令")
            # 刷新快照，保证同步到远程的始终是最新的本地信息
            _git_info_write_snapshot("${branch}" "${commit}")
        endif ()
    endif ()

    # 4) 兜底
    if (NOT branch)
        set(branch "unknown")
    endif ()
    if (NOT commit)
        set(commit "unknown")
    endif ()

    if (source)
        message(STATUS "Git info source: ${source}")
    else ()
        message(STATUS "Git info unavailable, fallback to \"unknown\"")
    endif ()

    set(${out_branch} "${branch}" PARENT_SCOPE)
    set(${out_commit} "${commit}" PARENT_SCOPE)
endfunction()
