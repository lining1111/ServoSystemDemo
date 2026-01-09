//
// Created by lining on 2023/2/20.
//

#include "GlogHelper.h"
#include <glog/logging.h>
#include "FileCleaner.h"
#include <fstream>
#include <iostream>
#include <thread>
#include <utility>

#include "utils/utils.h"


GlogHelper::GlogHelper(std::string _program, int _keep, std::string _logDir,
                       bool _isSendSTDOUT) : program(std::move(_program)),
                                             keepDays(_keep), logDir(std::move(_logDir)), isSendSTDOUT(_isSendSTDOUT) {
    FLAGS_log_dir = logDir;
    printf("日志目录:%s\n", logDir.c_str());
    FLAGS_logbufsecs = 0;
    FLAGS_max_log_size = 300;
    FLAGS_stop_logging_if_full_disk = true;
    if (isSendSTDOUT) {
        FLAGS_logtostderr = true;
    }
    google::InitGoogleLogging(program.data());
    google::InstallFailureSignalHandler();

    isRun = true;
    tClean = std::thread(&GlogHelper::cleaner, this);
}

GlogHelper::~GlogHelper() {
    isRun = false;
    if (tClean.joinable()) {
        tClean.join();
    }
    google::ShutdownGoogleLogging();
}

int GlogHelper::cleaner(const GlogHelper *local) {
    if (local == nullptr) {
        return -1;
    }
    uint64_t keepSeconds = local->keepDays * 60 * 60 * 24;
    LOG(WARNING) << "start logfile cleaner";
    FileCleaner::Config config = {
        .path = local->logDir,
        .suffix = {".INFO.", ".WARNING.", ".ERROR.", ".FATAL."},
        .keepSeconds = keepSeconds
    };

    FileCleaner fileCleaner(config);

    while (local->isRun) {
        std::this_thread::sleep_for(std::chrono::milliseconds(local->scanPeriod * 1000));
        fileCleaner.run();
    }
    LOG(WARNING) << "关闭日志定时清理任务";
    return 0;
}
