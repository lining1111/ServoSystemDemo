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
    m_logPath = path;
    //控制台 sink：不输出到控制台时直接关闭，即使被文件 logger 挂载也不会输出
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(isSendSTDOUT ? spdlog::level::debug : spdlog::level::off);

    // key 为 logger 名称，value 为 (文件名, 最大文件大小)；文件名为空表示仅控制台
    const std::map<std::string, std::tuple<std::string, size_t>> configs = {
            {"biz",  {"biz.log",  1024 * 1024 * 10}},
            {"tcp",  {"tcp.log",  1024 * 1024 * 10}},
            {"ws",   {"ws.log",   1024 * 1024 * 10}},
            {"http", {"http.log", 1024 * 1024}},
            {"mqtt", {"mqtt.log", 1024 * 1024}},
    };

    //常规文件 logger，isSendSTDOUT 时挂载控制台 sink 同步上屏
    auto makeFileLogger = [&](const std::string &name, const std::string &file, size_t maxSize)
            -> std::shared_ptr<spdlog::logger> {
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                m_logPath + "/" + file, maxSize, 5);
        if (isSendSTDOUT) {
            return std::make_shared<spdlog::logger>(name, spdlog::sinks_init_list{file_sink, console_sink});
        }
        return std::make_shared<spdlog::logger>(name, file_sink);
    };

    loggers_ = {{"console", std::make_shared<spdlog::logger>("console", console_sink)}};
    for (const auto &[key, cfg]: configs) {
        const auto &[file, maxSize] = cfg;
        loggers_[key] = makeFileLogger(key, file, maxSize);
    }

    //[%s:%#] 输出调用点的 文件名:行号（由 LOG_TO/各宏在调用点捕获 source_loc）
    SetPatternAll("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%s:%#] %v");
    SetLevelAll(spdlog::level::debug);
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
