#[[
    cmake/find_dependency_lib.cmake

    统一的第三方依赖查找函数：支持 AUTO / CMAKE_ONLY / PKG_ONLY 三种查找方式。
    查找成功后按 CMake find_package 的习惯产出一个统一命名的 imported target：<name>::<name>，
    同时把结果统一命名为 <PREFIX>_XXX 变量（PREFIX = 库名大写，变量名不能用 ::）。
    查找方式、配置文件位置、版本、头文件、库路径、统一命名都会打印出来。

    用法（CMAKE_MODULE_PATH 已包含 cmake 目录，直接 include 即可）：

        include(find_dependency_lib)

        find_dependency_lib(gRPC
                MODE AUTO                 # AUTO | CMAKE_ONLY | PKG_ONLY
                CMAKE_NAMES gRPC          # find_package(<name> CONFIG) 的包名，可多个按顺序尝试
                PKG_NAMES grpc++          # pkg-config 模块名，可多个（需全部命中）
                PKG_CANDIDATES aravis-0.8 aravis  # 备选模块名，按顺序试，命中第一个即止
                                                  # （aravis 只有 aravis-0.8.pc，写 aravis 必然找不到）
                TARGETS gRPC::grpc++      # 命中 CMake config 时使用的真实 imported target
                VERSION_MIN 1.30.0        # 可选：最低版本要求
                OUTPUT_TARGET gRPC::gRPC  # 可选：自定义统一 target 名，默认 <name>::<name>
                REQUIRED)                 # 可选：找不到时 FATAL_ERROR，否则只 WARNING

        # 统一 target 用法与 find_package 的 imported target 完全一致
        target_link_libraries(GigECameraTest PRIVATE gRPC::gRPC)

    统一 target（即“库依赖的命名”，和 CMake 官方保持一致）：
        gRPC       -> gRPC::gRPC        （默认取 <name>::<name>，可用 OUTPUT_TARGET 覆盖）
        protobuf   -> protobuf::protobuf
        若该名字的 target 已存在（例如 config 包自己就提供了 Foo::Foo），则直接复用，不重复创建。

    统一变量输出（以 gRPC 为例，PREFIX = GRPC）：
        GRPC_FOUND            是否找到（TRUE/FALSE）
        GRPC_PROVIDER         命中方式：cmake / pkgconfig / none
        GRPC_VERSION          版本号
        GRPC_INCLUDE_DIRS     头文件目录
        GRPC_LIBRARY_DIRS     库目录
        GRPC_LIBRARIES        库：cmake 方式为 imported target，pkg-config 方式为库名
        GRPC_LINK_LIBRARIES   pkg-config 给出的绝对路径库列表（cmake 方式为空）
        GRPC_TARGETS          命中的 imported target 列表
        GRPC_LINK_TARGET      统一 target 名（如 gRPC::gRPC），找不到时为空
        GRPC_CONFIG_FILE      CMake config 文件（找不到具体文件时为其所在目录）
        GRPC_PC_FILE          命中的 .pc 文件

    可选开关：
        QUIET      不打印查找信息（失败也只在未指定 REQUIRED 时静默）
        NO_PRINT   只取变量，不打印任何查找信息
]]

include_guard(GLOBAL)

function(find_dependency_lib _fdl_name)
    # 参数解析：布尔开关 / 单值 / 多值
    cmake_parse_arguments(FDL
            "REQUIRED;QUIET;NO_PRINT"
            "MODE;VERSION_MIN;OUTPUT_TARGET"
            "CMAKE_NAMES;PKG_NAMES;PKG_CANDIDATES;TARGETS;CMAKE_PREFIX_HINTS"
            ${ARGN})

    if (NOT _fdl_name)
        message(FATAL_ERROR "find_dependency_lib(): 缺少库名参数，用法 find_dependency_lib(<name> MODE <AUTO|CMAKE_ONLY|PKG_ONLY> ...)")
    endif ()

    # 查找方式：默认 AUTO，大小写不敏感
    if (NOT FDL_MODE)
        set(FDL_MODE AUTO)
    endif ()
    string(TOUPPER "${FDL_MODE}" _fdl_mode)
    if (NOT _fdl_mode MATCHES "^(AUTO|CMAKE_ONLY|PKG_ONLY)$")
        message(FATAL_ERROR "find_dependency_lib(${_fdl_name}): MODE=${FDL_MODE} 无效，只能是 AUTO / CMAKE_ONLY / PKG_ONLY")
    endif ()

    # 统一变量前缀：库名转大写，例如 gRPC -> GRPC，输出 GRPC_FOUND / GRPC_LIBRARIES ...
    string(TOUPPER "${_fdl_name}" _fdl_prefix)

    # 统一 target 名：模仿 find_package 的 <name>::<name> 形式，便于 link 时一眼看出依赖
    if (FDL_OUTPUT_TARGET)
        string(REGEX REPLACE "[^A-Za-z0-9_:.+^-]" "_" _fdl_target_name "${FDL_OUTPUT_TARGET}")
    else ()
        string(REGEX REPLACE "[^A-Za-z0-9_:.+^-]" "_" _fdl_target_name "${_fdl_name}::${_fdl_name}")
    endif ()

    # AUTO 两种方式都试（先 cmake 后 pkg-config），其余模式只走指定的一条路
    set(_fdl_try_cmake FALSE)
    set(_fdl_try_pkg FALSE)
    if (_fdl_mode STREQUAL "CMAKE_ONLY")
        set(_fdl_try_cmake TRUE)
    elseif (_fdl_mode STREQUAL "PKG_ONLY")
        set(_fdl_try_pkg TRUE)
    else ()
        set(_fdl_try_cmake TRUE)
        set(_fdl_try_pkg TRUE)
    endif ()

    # 走哪条路就必须给对应的名字，否则后面必然找不到，提前报到参数位置更好排查
    if (_fdl_try_cmake AND NOT FDL_CMAKE_NAMES)
        message(FATAL_ERROR "find_dependency_lib(${_fdl_name}): MODE=${_fdl_mode} 需要 CMAKE_NAMES（find_package 的包名）")
    endif ()
    if (_fdl_try_pkg AND NOT FDL_PKG_NAMES AND NOT FDL_PKG_CANDIDATES)
        message(FATAL_ERROR "find_dependency_lib(${_fdl_name}): MODE=${_fdl_mode} 需要 PKG_NAMES 或 PKG_CANDIDATES（pkg-config 的模块名）")
    endif ()

    # 探测结果集中存放，最后统一输出，避免中途 return 漏设变量
    set(_fdl_found FALSE)
    set(_fdl_provider "none")
    set(_fdl_version "")
    set(_fdl_includes "")
    set(_fdl_libdirs "")
    set(_fdl_libs "")
    set(_fdl_link_libs "")
    set(_fdl_targets "")
    set(_fdl_config_file "")
    set(_fdl_pc_file "")

    # ------------------------------------------------------------------
    # 1) CMake config 方式：find_package(<pkg> CONFIG)
    # ------------------------------------------------------------------
    if (_fdl_try_cmake)
        foreach (_pkg IN LISTS FDL_CMAKE_NAMES)
            if (_fdl_found)
                break()
            endif ()
            # 只走 CONFIG 模式：apt 安装的库通常没有 *Config.cmake，QUIET 让失败保持安静，另行兜底
            find_package(${_pkg} CONFIG QUIET HINTS ${FDL_CMAKE_PREFIX_HINTS})

            set(_pkg_found_var "${_pkg}_FOUND")
            if (DEFINED ${_pkg_found_var})
                if (${_pkg_found_var})
                    set(_fdl_found TRUE)
                    set(_fdl_provider "cmake")

                    # config 文件位置：find_package 会设置 <Pkg>_DIR 指向包含 *Config.cmake 的目录，
                    # 再 glob 一次拿到具体文件名，方便直接把配置来源打印出来
                    set(_pkg_dir_var "${_pkg}_DIR")
                    if (DEFINED ${_pkg_dir_var})
                        set(_fdl_config_file "${${_pkg_dir_var}}")
                        file(GLOB _cfg_files
                                "${${_pkg_dir_var}}/${_pkg}Config.cmake"
                                "${${_pkg_dir_var}}/${_pkg}-config.cmake"
                                "${${_pkg_dir_var}}/${_pkg}ConfigVersion.cmake")
                        if (_cfg_files)
                            list(GET _cfg_files 0 _fdl_config_file)
                        endif ()
                    endif ()

                    set(_pkg_ver_var "${_pkg}_VERSION")
                    if (DEFINED ${_pkg_ver_var})
                        set(_fdl_version "${${_pkg_ver_var}}")
                    endif ()
                    set(_pkg_inc_var "${_pkg}_INCLUDE_DIRS")
                    if (DEFINED ${_pkg_inc_var})
                        set(_fdl_includes "${${_pkg_inc_var}}")
                    endif ()
                    set(_pkg_libdir_var "${_pkg}_LIBRARY_DIRS")
                    if (DEFINED ${_pkg_libdir_var})
                        set(_fdl_libdirs "${${_pkg_libdir_var}}")
                    endif ()

                    # 优先用真实 imported target（它自带 include/依赖传递），没有再用 <Pkg>_LIBRARIES
                    foreach (_tgt IN LISTS FDL_TARGETS)
                        if (TARGET ${_tgt})
                            list(APPEND _fdl_targets ${_tgt})
                        endif ()
                    endforeach ()
                    if (_fdl_targets)
                        set(_fdl_libs ${_fdl_targets})
                    else ()
                        set(_pkg_libs_var "${_pkg}_LIBRARIES")
                        if (DEFINED ${_pkg_libs_var})
                            set(_fdl_libs "${${_pkg_libs_var}}")
                        endif ()
                    endif ()
                endif ()
            endif ()
        endforeach ()
    endif ()

    # ------------------------------------------------------------------
    # 2) pkg-config 方式（本地 apt 安装的库一般走这里）
    # ------------------------------------------------------------------
    if (NOT _fdl_found AND _fdl_try_pkg)
        find_package(PkgConfig QUIET)
        if (PKG_CONFIG_FOUND)
            # 每个库用独立前缀（GRPC + PKG = GRPC_PKG），避免和别的模块互相覆盖变量
            set(_pkg_prefix "${_fdl_prefix}PKG")
            set(_fdl_pkg_modules "")

            if (FDL_PKG_NAMES)
                # PKG_NAMES 语义：列出的模块必须全部命中
                pkg_check_modules(${_pkg_prefix} QUIET ${FDL_PKG_NAMES})
                if (${_pkg_prefix}_FOUND)
                    set(_fdl_pkg_modules ${FDL_PKG_NAMES})
                endif ()
            else ()
                # PKG_CANDIDATES 语义：按顺序逐个尝试，命中第一个即止。
                # 用于模块名带版本后缀、写死名字必然找不到的库（如 aravis 只有 aravis-0.8.pc）
                foreach (_cand IN LISTS FDL_PKG_CANDIDATES)
                    if (_fdl_pkg_modules)
                        break()
                    endif ()
                    pkg_check_modules(${_pkg_prefix} QUIET ${_cand})
                    if (${_pkg_prefix}_FOUND)
                        set(_fdl_pkg_modules ${_cand})
                    endif ()
                endforeach ()
            endif ()

            if (_fdl_pkg_modules)
                set(_fdl_found TRUE)
                set(_fdl_provider "pkgconfig")

                set(_ver_var "${_pkg_prefix}_VERSION")
                if (DEFINED ${_ver_var})
                    set(_fdl_version "${${_ver_var}}")
                endif ()
                set(_inc_var "${_pkg_prefix}_INCLUDE_DIRS")
                if (DEFINED ${_inc_var})
                    set(_fdl_includes "${${_inc_var}}")
                endif ()
                set(_libdir_var "${_pkg_prefix}_LIBRARY_DIRS")
                if (DEFINED ${_libdir_var})
                    set(_fdl_libdirs "${${_libdir_var}}")
                endif ()
                set(_libs_var "${_pkg_prefix}_LIBRARIES")
                if (DEFINED ${_libs_var})
                    set(_fdl_libs "${${_libs_var}}")
                endif ()
                set(_link_var "${_pkg_prefix}_LINK_LIBRARIES")
                if (DEFINED ${_link_var})
                    set(_fdl_link_libs "${${_link_var}}")
                endif ()

                list(GET _fdl_pkg_modules 0 _first_module)

                # pkg-config 会过滤掉系统默认路径（如 /usr/include、/usr/lib），
                # 导致 INCLUDE_DIRS / LIBRARY_DIRS 为空，这里用 --variable 兜底取一次，
                # 否则后面创建统一 target 时没有 include 目录可用
                if (_fdl_includes STREQUAL "")
                    execute_process(
                            COMMAND ${PKG_CONFIG_EXECUTABLE} --variable=includedir ${_first_module}
                            OUTPUT_VARIABLE _pc_incdir
                            OUTPUT_STRIP_TRAILING_WHITESPACE
                            ERROR_QUIET)
                    if (_pc_incdir)
                        set(_fdl_includes "${_pc_incdir}")
                    endif ()
                endif ()
                if (_fdl_libdirs STREQUAL "")
                    execute_process(
                            COMMAND ${PKG_CONFIG_EXECUTABLE} --variable=libdir ${_first_module}
                            OUTPUT_VARIABLE _pc_libdir
                            OUTPUT_STRIP_TRAILING_WHITESPACE
                            ERROR_QUIET)
                    if (_pc_libdir)
                        set(_fdl_libdirs "${_pc_libdir}")
                    endif ()
                endif ()

                # .pc 文件位置：pkg_check_modules 不提供，用 pcfiledir 变量反查；
                # 模块名与文件名不一致时再 glob 兜底（如 grpc++ -> grpc++.pc）
                execute_process(
                        COMMAND ${PKG_CONFIG_EXECUTABLE} --variable=pcfiledir ${_first_module}
                        OUTPUT_VARIABLE _pcdir
                        OUTPUT_STRIP_TRAILING_WHITESPACE
                        ERROR_QUIET)
                if (_pcdir)
                    set(_fdl_pc_file "${_pcdir}/${_first_module}.pc")
                    if (NOT EXISTS "${_fdl_pc_file}")
                        file(GLOB _pc_files "${_pcdir}/*${_first_module}*.pc")
                        if (_pc_files)
                            list(GET _pc_files 0 _fdl_pc_file)
                        endif ()
                    endif ()
                endif ()
            endif ()
        endif ()
    endif ()

    # ------------------------------------------------------------------
    # 3) 版本校验 + 失败处理
    # ------------------------------------------------------------------
    if (_fdl_found AND FDL_VERSION_MIN)
        if (_fdl_version AND _fdl_version VERSION_LESS FDL_VERSION_MIN)
            message(WARNING "find_dependency_lib(${_fdl_name}): 找到的版本 ${_fdl_version} 低于要求的 ${FDL_VERSION_MIN}"
                    "（provider=${_fdl_provider}）")
            # 只把 found/provider 置为否，版本/路径等信息保留，打印时仍可看出找到的是哪一份
            set(_fdl_found FALSE)
            set(_fdl_provider "none")
        endif ()
    endif ()

    if (NOT _fdl_found)
        set(_fdl_msg "find_dependency_lib(${_fdl_name}): 未找到该依赖"
                "（MODE=${_fdl_mode}，CMAKE_NAMES=${FDL_CMAKE_NAMES}，PKG_NAMES=${FDL_PKG_NAMES}）")
        if (FDL_REQUIRED)
            message(FATAL_ERROR ${_fdl_msg})
        elseif (NOT FDL_QUIET)
            message(WARNING ${_fdl_msg})
        endif ()
    endif ()

    # ------------------------------------------------------------------
    # 4) 生成统一命名的 imported target：<name>::<name>
    # ------------------------------------------------------------------
    # 注意：不能对已有的 imported target 用 add_library(ALIAS)（3.18 之前不支持，且要求 GLOBAL），
    # 因此这里建一个 INTERFACE IMPORTED target 作为“统一门面”，把真实 target / 库名转发到链接属性上。
    # imported target 属于目录作用域（不像函数内变量会消失），函数返回后调用方仍可使用。
    set(_fdl_link_target "")
    if (_fdl_found)
        if (TARGET ${_fdl_target_name})
            # 已存在同名 target（例如 config 包自己就提供了 Foo::Foo），直接复用
            set(_fdl_link_target "${_fdl_target_name}")
        else ()
            add_library(${_fdl_target_name} INTERFACE IMPORTED)

            # 过滤不存在的目录，避免 INTERFACE_INCLUDE_DIRECTORIES 指向无效路径时报错
            set(_fdl_tgt_includes "")
            foreach (_dir IN LISTS _fdl_includes)
                if (EXISTS "${_dir}")
                    list(APPEND _fdl_tgt_includes "${_dir}")
                endif ()
            endforeach ()
            set(_fdl_tgt_libdirs "")
            foreach (_dir IN LISTS _fdl_libdirs)
                if (IS_DIRECTORY "${_dir}")
                    list(APPEND _fdl_tgt_libdirs "${_dir}")
                endif ()
            endforeach ()

            if (_fdl_tgt_includes)
                set_target_properties(${_fdl_target_name} PROPERTIES
                        INTERFACE_INCLUDE_DIRECTORIES "${_fdl_tgt_includes}")
            endif ()
            # pkg-config 方式拿到的是库名（-lxxx），必须同时传库目录才能链接到非标准前缀下的库
            if (_fdl_tgt_libdirs AND NOT _fdl_targets)
                set_target_properties(${_fdl_target_name} PROPERTIES
                        INTERFACE_LINK_DIRECTORIES "${_fdl_tgt_libdirs}")
            endif ()
            if (_fdl_libs)
                set_target_properties(${_fdl_target_name} PROPERTIES
                        INTERFACE_LINK_LIBRARIES "${_fdl_libs}")
            endif ()
            set(_fdl_link_target "${_fdl_target_name}")
        endif ()
    endif ()

    # ------------------------------------------------------------------
    # 5) 统一命名输出到调用者作用域
    #    函数内 set 的变量默认不会传出去，必须显式 PARENT_SCOPE
    # ------------------------------------------------------------------
    set(${_fdl_prefix}_FOUND ${_fdl_found} PARENT_SCOPE)
    set(${_fdl_prefix}_PROVIDER "${_fdl_provider}" PARENT_SCOPE)
    set(${_fdl_prefix}_VERSION "${_fdl_version}" PARENT_SCOPE)
    set(${_fdl_prefix}_INCLUDE_DIRS "${_fdl_includes}" PARENT_SCOPE)
    set(${_fdl_prefix}_LIBRARY_DIRS "${_fdl_libdirs}" PARENT_SCOPE)
    set(${_fdl_prefix}_LIBRARIES "${_fdl_libs}" PARENT_SCOPE)
    set(${_fdl_prefix}_LINK_LIBRARIES "${_fdl_link_libs}" PARENT_SCOPE)
    set(${_fdl_prefix}_TARGETS "${_fdl_targets}" PARENT_SCOPE)
    set(${_fdl_prefix}_LINK_TARGET "${_fdl_link_target}" PARENT_SCOPE)
    set(${_fdl_prefix}_CONFIG_FILE "${_fdl_config_file}" PARENT_SCOPE)
    set(${_fdl_prefix}_PC_FILE "${_fdl_pc_file}" PARENT_SCOPE)

    # ------------------------------------------------------------------
    # 6) 打印查找结果（方式、配置文件、版本、头文件、库路径、统一命名）
    # ------------------------------------------------------------------
    if (NOT FDL_NO_PRINT AND NOT FDL_QUIET)
        # 空值统一显示成 "-"，避免打印出空白行看不出是“没取到”还是“本来就空”
        foreach (_slot _fdl_version _fdl_includes _fdl_libdirs _fdl_libs _fdl_link_libs _fdl_targets _fdl_config_file _fdl_pc_file)
            if ("${${_slot}}" STREQUAL "")
                set(${_slot} "-")
            endif ()
        endforeach ()

        # 来源信息优先展示命中的 config / .pc 文件；版本校验失败时也保留，方便排查
        if (_fdl_provider STREQUAL "cmake")
            set(_fdl_src "${_fdl_config_file}")
        elseif (_fdl_provider STREQUAL "pkgconfig")
            set(_fdl_src "${_fdl_pc_file}")
        elseif (NOT "${_fdl_config_file}" STREQUAL "-")
            set(_fdl_src "${_fdl_config_file}")
        elseif (NOT "${_fdl_pc_file}" STREQUAL "-")
            set(_fdl_src "${_fdl_pc_file}")
        else ()
            set(_fdl_src "-")
        endif ()

        if (_fdl_link_target)
            set(_fdl_usage "target_link_libraries(<your_target> PRIVATE ${_fdl_link_target})")
        else ()
            set(_fdl_usage "-")
        endif ()

        message(STATUS "======================================================================")
        message(STATUS "[find_dependency_lib] ${_fdl_name}")
        message(STATUS "  查找方式 (MODE)          : ${_fdl_mode}")
        message(STATUS "  命中途径 (PROVIDER)      : ${_fdl_provider}")
        message(STATUS "  配置文件 (CONFIG_FILE)   : ${_fdl_config_file}")
        message(STATUS "  pc 文件  (PC_FILE)       : ${_fdl_pc_file}")
        message(STATUS "  版本     (VERSION)       : ${_fdl_version}")
        message(STATUS "  头文件   (INCLUDE_DIRS)  : ${_fdl_includes}")
        message(STATUS "  库目录   (LIBRARY_DIRS)  : ${_fdl_libdirs}")
        message(STATUS "  库       (LIBRARIES)     : ${_fdl_libs}")
        message(STATUS "  绝对库   (LINK_LIBRARIES): ${_fdl_link_libs}")
        message(STATUS "  targets  (TARGETS)       : ${_fdl_targets}")
        message(STATUS "  统一命名 (LINK_TARGET)   : ${_fdl_usage}")
        message(STATUS "  统一变量 : ${_fdl_prefix}_FOUND / ${_fdl_prefix}_PROVIDER / ${_fdl_prefix}_VERSION"
                " / ${_fdl_prefix}_INCLUDE_DIRS / ${_fdl_prefix}_LIBRARY_DIRS / ${_fdl_prefix}_LIBRARIES"
                " / ${_fdl_prefix}_LINK_LIBRARIES / ${_fdl_prefix}_TARGETS / ${_fdl_prefix}_LINK_TARGET"
                " / ${_fdl_prefix}_CONFIG_FILE / ${_fdl_prefix}_PC_FILE")
        message(STATUS "  来源信息 : ${_fdl_src}")
        message(STATUS "======================================================================")
    endif ()
endfunction()
