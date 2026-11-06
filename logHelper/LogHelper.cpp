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
    console_logger_ = spdlog::stdout_color_mt("console");
    console_logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
    //不输出到控制台时关闭 console logger
    console_logger_->set_level(isSendSTDOUT ? spdlog::level::debug : spdlog::level::off);

    auto level = spdlog::level::debug;
    m_logPath = path;
    //按路径生成日志文件logger（轮转文件：单文件10MB，最多保留5个）
    biz_logger_ = spdlog::rotating_logger_mt("biz", m_logPath + "/biz.log", 1024 * 1024 * 10, 5);
    tcp_logger_ = spdlog::rotating_logger_mt("tcp", m_logPath + "/tcp.log", 1024 * 1024 * 10, 5);
    ws_logger_ = spdlog::rotating_logger_mt("ws", m_logPath + "/ws.log", 1024 * 1024 * 10, 5);
    http_logger_ = spdlog::rotating_logger_mt("http", m_logPath + "/http.log", 1024 * 1024 * 10, 5);

    SetPatternAll("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
    SetLevelAll(level);
}

void LogHelper::SetPatternAll(const std::string &pattern) const {
    console_logger_->set_pattern(pattern);
    biz_logger_->set_pattern(pattern);
    tcp_logger_->set_pattern(pattern);
    ws_logger_->set_pattern(pattern);
    http_logger_->set_pattern(pattern);
}

void LogHelper::SetLevelAll(spdlog::level::level_enum level) const {
    console_logger_->set_level(level);
    biz_logger_->set_level(level);
    tcp_logger_->set_level(level);
    ws_logger_->set_level(level);
    http_logger_->set_level(level);
}
