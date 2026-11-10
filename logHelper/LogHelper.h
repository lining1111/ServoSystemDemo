//
// Created by lining on 2026/11/6.
//
// 日志模块（对齐 StorageService 的 logHelper 逻辑）：
//   - 单例 + 按模块分文件落盘（map 管理）：biz.log / tcp.log / ws.log / http.log
//   - 文件按大小滚动（单文件 10MB，最多 5 个备份）
//   - isSendSTDOUT 时文件日志同步输出到控制台（多 sink）
//   - Close() 刷盘并关闭文件句柄，之后日志调用安全空转；Init 可重复调用
//   - BIZ_LOG/TCP_LOG/WS_LOG/HTTP_LOG 宏在调用点捕获 文件名:行号（对齐 zap AddCaller）
//     写入 pattern 的 [%s:%#]

#ifndef LOGHELPER_H
#define LOGHELPER_H

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <cstring>

#include "utils/nonCopyable.hpp"
#include "spdlog/spdlog.h"
#include "spdlog/fmt/fmt.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

// 取路径中的文件名部分（不含目录）
inline const char *LogBaseName(const char *path) {
    const char *base = path;
    for (const char *p = path; *p != '\0'; ++p) {
        if (*p == '/') {
            base = p + 1;
        }
    }
    return base;
}

// 格式串类型：
//   fmt >= 8.0（spdlog >= 1.8）有编译期检查的 fmt::format_string<Args...>；
//   老环境没有该类型时退回 const char*，
//   此时格式串只能是编译期字符串常量，但没有编译期格式校验。
#if defined(FMT_VERSION) && FMT_VERSION >= 80000
template<typename... Args>
using FmtString = fmt::format_string<Args...>;
#else
template<typename... Args>
using FmtString = const char *;
#endif

// type_identity 包装：让函数签名中的 Args 成为非推导上下文，
// 模板参数只从可变参数包(args...)推导（与 fmt::format_string 的实现方式一致）
template<typename T>
struct LogTypeIdentity {
    using type = T;
};

template<typename... Args>
using LogFmt = typename LogTypeIdentity<FmtString<Args...>>::type;

class LogHelper : public NonCopyable {
public:
    static LogHelper *m_pInstance;

    static LogHelper *Instance();

    //按名称取 logger，不存在（未调用 Init/已 Close 或名称错误）时返回 nullptr
    //供各业务类构造时获取模块 logger 存为成员（如 tcp::Server::log_）
    std::shared_ptr<spdlog::logger> GetLogger(const std::string &name) const {
        auto iter = loggers_.find(name);
        if (iter == loggers_.end()) {
            return nullptr;
        }
        return iter->second;
    }

private:
    std::string m_logPath;
    //logger 集合，key：console / biz / tcp / ws / http
    //biz / tcp / ws / http 分别写 biz.log / tcp.log / ws.log / http.log，isSendSTDOUT 时同步输出到控制台
    std::map<std::string, std::shared_ptr<spdlog::logger>> loggers_;
    //Init/Close 的互斥锁，保证重复 Init 与退出 Close 的并发安全
    std::mutex initMtx_;

    // 统一写日志：loc 为调用点位置（pattern 的 %s:%#），level 为日志等级，args 为格式化参数
    // 注意：格式串需为编译期常量；若格式串来自运行时拼接，调用处用 fmt::runtime(str) 包裹
    template<typename... Args>
    static void WriteLog(const std::shared_ptr<spdlog::logger> &logger,
                         spdlog::source_loc loc,
                         spdlog::level::level_enum level,
                         LogFmt<Args...> fmt,
                         Args &&... args) {
        // logger 未初始化(未调用 Init/已 Close)或该等级被过滤时直接返回，避免无谓格式化
        if (!logger || !logger->should_log(level)) {
            return;
        }
        logger->log(loc, level, fmt, std::forward<Args>(args)...);
        logger->flush();
    }

public:
    /**
     * 初始化日志
     * @param path 日志输出目录
     * @param isSendSTDOUT 是否同时输出到控制台
     * 可重复调用：会先刷盘并关闭上一次的文件句柄再重新创建
     */
    void Init(const std::string &path, bool isSendSTDOUT = false);

    /**
     * 刷盘并关闭所有文件句柄，之后所有日志调用安全空转
     * 进程退出前调用（对齐 StorageService 的 defer logHelper.Close()）
     */
    void Close();

    void SetPatternAll(const std::string &pattern) const;

    void SetLevelAll(spdlog::level::level_enum level) const;

    /**
     * 通用日志，按名称写入对应 logger（带调用点位置，供宏使用）
     * @param name logger 名：console / biz / tcp / ws / http / mqtt
     */
    template<typename... Args>
    void Log(const std::string &name, spdlog::source_loc loc, spdlog::level::level_enum level,
             LogFmt<Args...> fmt, Args &&... args) {
        WriteLog(GetLogger(name), loc, level, fmt, std::forward<Args>(args)...);
    }

    /**
     * 写入指定的 logger 实例（带调用点位置，供 LOG_TO 宏使用）
     * 业务类持有 logger 成员时用此入口；logger 为空（未 Init/已 Close）时安全空转
     */
    template<typename... Args>
    static void WriteTo(const std::shared_ptr<spdlog::logger> &logger, spdlog::source_loc loc,
                        spdlog::level::level_enum level, LogFmt<Args...> fmt, Args &&... args) {
        WriteLog(logger, loc, level, fmt, std::forward<Args>(args)...);
    }

    /**
     * 控制台日志（无调用点位置）
     */
    template<typename... Args>
    void ConsoleLog(spdlog::level::level_enum level, LogFmt<Args...> fmt, Args &&... args) {
        WriteLog(GetLogger("console"), {}, level, fmt, std::forward<Args>(args)...);
    }

    /**
     * 业务日志，写 biz.log（无调用点位置，新代码建议用 BIZ_LOG 宏）
     */
    template<typename... Args>
    void BizLog(spdlog::level::level_enum level, LogFmt<Args...> fmt, Args &&... args) {
        WriteLog(GetLogger("biz"), {}, level, fmt, std::forward<Args>(args)...);
    }

    /**
     * TCP 报文日志，写 tcp.log（无调用点位置，新代码建议用 TCP_LOG 宏）
     */
    template<typename... Args>
    void TcpLog(spdlog::level::level_enum level, LogFmt<Args...> fmt, Args &&... args) {
        WriteLog(GetLogger("tcp"), {}, level, fmt, std::forward<Args>(args)...);
    }

    /**
     * WebSocket 日志，写 ws.log（无调用点位置，新代码建议用 WS_LOG 宏）
     */
    template<typename... Args>
    void WsLog(spdlog::level::level_enum level, LogFmt<Args...> fmt, Args &&... args) {
        WriteLog(GetLogger("ws"), {}, level, fmt, std::forward<Args>(args)...);
    }

    /**
     * HTTP/下载 日志，写 http.log（无调用点位置，新代码建议用 HTTP_LOG 宏）
     */
    template<typename... Args>
    void HttpLog(spdlog::level::level_enum level, LogFmt<Args...> fmt, Args &&... args) {
        WriteLog(GetLogger("http"), {}, level, fmt, std::forward<Args>(args)...);
    }
};

// 日志宏：在调用点捕获 文件名:行号 写入日志（对齐 zap AddCaller），
// 用法与 BizLog/TcpLog/... 一致：BIZ_LOG(spdlog::level::info, "msg {}", val)
#define BIZ_LOG(level, ...) \
    LogHelper::Instance()->Log("biz", spdlog::source_loc{LogBaseName(__FILE__), __LINE__, nullptr}, level, __VA_ARGS__)

#define TCP_LOG(level, ...) \
    LogHelper::Instance()->Log("tcp", spdlog::source_loc{LogBaseName(__FILE__), __LINE__, nullptr}, level, __VA_ARGS__)

#define WS_LOG(level, ...) \
    LogHelper::Instance()->Log("ws", spdlog::source_loc{LogBaseName(__FILE__), __LINE__, nullptr}, level, __VA_ARGS__)

#define HTTP_LOG(level, ...) \
    LogHelper::Instance()->Log("http", spdlog::source_loc{LogBaseName(__FILE__), __LINE__, nullptr}, level, __VA_ARGS__)

#define CONSOLE_LOG(level, ...) \
    LogHelper::Instance()->Log("console", spdlog::source_loc{LogBaseName(__FILE__), __LINE__, nullptr}, level, __VA_ARGS__)

// 日志宏（logger 实例版）：业务类持有 logger 成员时使用，
// 如 LOG_TO(log_, spdlog::level::info, "msg {}", val)；logger 为空时安全空转
#define LOG_TO(logger, level, ...) \
    LogHelper::WriteTo(logger, spdlog::source_loc{LogBaseName(__FILE__), __LINE__, nullptr}, level, __VA_ARGS__)

#endif //LOGHELPER_H
