//
// Created by lining on 2026/11/6.
//

#ifndef LOGHELPER_H
#define LOGHELPER_H

#include <memory>
#include <string>

#include "utils/nonCopyable.hpp"
#include "spdlog/spdlog.h"
#include "spdlog/fmt/fmt.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

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

class LogHelper : public NonCopyable {
public:
    static LogHelper *m_pInstance;

    static LogHelper *Instance();

private:
    std::string m_logPath;
    std::shared_ptr<spdlog::logger> console_logger_;
    std::shared_ptr<spdlog::logger> biz_logger_;
    std::shared_ptr<spdlog::logger> tcp_logger_;
    std::shared_ptr<spdlog::logger> ws_logger_;
    std::shared_ptr<spdlog::logger> http_logger_;

    // 统一写日志：level 为日志等级，fmt 为 fmt 风格格式串，args 为格式化参数
    // 注意：格式串需为编译期常量；若格式串来自运行时拼接，调用处用 fmt::runtime(str) 包裹
    template<typename... Args>
    static void WriteLog(const std::shared_ptr<spdlog::logger> &logger,
                         spdlog::level::level_enum level,
                         FmtString<Args...> fmt,
                         Args &&... args) {
        // logger 未初始化(未调用 Init)或该等级被过滤时直接返回，避免无谓格式化
        if (!logger || !logger->should_log(level)) {
            return;
        }
        logger->log(level, fmt, std::forward<Args>(args)...);
        logger->flush();
    }

public:
    /**
     * 初始化日志
     * @param path 日志输出目录
     * @param isSendSTDOUT 是否同时输出到控制台
     */
    void Init(const std::string &path, bool isSendSTDOUT = false);

    void SetPatternAll(const std::string &pattern) const;

    void SetLevelAll(spdlog::level::level_enum level) const;

    /**
     * 控制台日志
     */
    template<typename... Args>
    void ConsoleLog(spdlog::level::level_enum level, FmtString<Args...> fmt, Args &&... args) {
        WriteLog(console_logger_, level, fmt, std::forward<Args>(args)...);
    }

    /**
     * 业务日志，写 biz.log
     * @param level 日志等级，如 spdlog::level::info
     * @param fmt 格式串，如 "recv cmd: {}, value: {}"
     * @param args 若干可变参数
     */
    template<typename... Args>
    void BizLog(spdlog::level::level_enum level, FmtString<Args...> fmt, Args &&... args) {
        WriteLog(biz_logger_, level, fmt, std::forward<Args>(args)...);
    }

    /**
     * TCP 报文日志，写 tcp.log
     */
    template<typename... Args>
    void TcpLog(spdlog::level::level_enum level, FmtString<Args...> fmt, Args &&... args) {
        WriteLog(tcp_logger_, level, fmt, std::forward<Args>(args)...);
    }

    /**
     * WebSocket 日志，写 ws.log
     */
    template<typename... Args>
    void WsLog(spdlog::level::level_enum level, FmtString<Args...> fmt, Args &&... args) {
        WriteLog(ws_logger_, level, fmt, std::forward<Args>(args)...);
    }

    /**
     * HTTP/下载 日志，写 http.log
     */
    template<typename... Args>
    void HttpLog(spdlog::level::level_enum level, FmtString<Args...> fmt, Args &&... args) {
        WriteLog(http_logger_, level, fmt, std::forward<Args>(args)...);
    }
};


#endif //LOGHELPER_H
