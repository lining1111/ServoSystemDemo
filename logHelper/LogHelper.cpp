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
    auto level = spdlog::level::debug;
    m_logPath = path;
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
    };

    SetPatternAll("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
    SetLevelAll(level);
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
