//
// Created by lining on 2026/11/6.
//

#include "LogHelper.h"

LogHelper *LogHelper::m_pInstance = nullptr;

LogHelper *LogHelper::Instance() {
    if (m_pInstance == nullptr) {
        m_pInstance = new LogHelper();
    }
    return m_pInstance;
}

void LogHelper::Init(const std::string &path, bool isSendSTDOUT) {
    std::lock_guard<std::mutex> lock(initMtx_);
    auto level = spdlog::level::debug;
    m_logPath = path;

    //可重复调用：先刷盘并丢弃上一次的 logger（rotating_file_sink 析构时关闭文件句柄）
    for (const auto &iter: loggers_) {
        if (iter.second) {
            iter.second->flush();
        }
    }
    loggers_.clear();

    //控制台 sink：不输出到控制台时直接关闭，即使被文件 logger 挂载也不会输出
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(isSendSTDOUT ? level : spdlog::level::off);

    //常规文件 logger（轮转文件：单文件10MB，最多保留5个），isSendSTDOUT 时挂载控制台 sink 同步上屏
    auto makeFileLogger = [&](const std::string &name) -> std::shared_ptr<spdlog::logger> {
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            m_logPath + "/" + name + ".log", 1024 * 1024 * 10, 5);
        if (isSendSTDOUT) {
            return std::make_shared<spdlog::logger>(name, spdlog::sinks_init_list{file_sink, console_sink});
        }
        return std::make_shared<spdlog::logger>(name, file_sink);
    };

    loggers_ = {
        {"console", std::make_shared<spdlog::logger>("console", console_sink)},
        {"biz", makeFileLogger("biz")},
        {"tcp", makeFileLogger("tcp")},
        {"ws", makeFileLogger("ws")},
        {"http", makeFileLogger("http")},
        {"mqtt", makeFileLogger("mqtt")},
    };

    //[%s:%#] 输出调用点的 文件名:行号（由 FmtString 在调用点捕获），对齐 zap AddCaller
    SetPatternAll("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%s:%#] %v");
    SetLevelAll(level);
}

void LogHelper::Close() {
    std::lock_guard<std::mutex> lock(initMtx_);
    //刷盘所有 logger 后丢弃，文件句柄随 rotating_file_sink 析构关闭；
    //业务类可能仍持有 logger 成员（log_），这里同时把等级置 off 使写入空转；
    //GetLogger 返回 nullptr，宏调用同样安全空转
    for (const auto &iter: loggers_) {
        if (iter.second) {
            iter.second->flush();
            iter.second->set_level(spdlog::level::off);
        }
    }
    loggers_.clear();
}

void LogHelper::SetPatternAll(const std::string &pattern) const {
    for (const auto &iter: loggers_) {
        iter.second->set_pattern(pattern);
    }
}

void LogHelper::SetLevelAll(spdlog::level::level_enum level) const {
    for (const auto &iter: loggers_) {
        iter.second->set_level(level);
    }
}
