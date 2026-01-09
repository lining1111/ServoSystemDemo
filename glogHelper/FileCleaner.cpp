//
// Created by lining on 2026/1/9.
//

#include "FileCleaner.h"

#include <cstring>
#include <iostream>
#include <sys/stat.h>

#include "utils/utils.h"

FileCleaner::FileCleaner(Config config) {
    localConfig = config;
}

FileCleaner::~FileCleaner() {
}

void FileCleaner::run() {
    if (localConfig.keepSeconds > 0) {
        //找到指定目录下符合条件的文件集合
        std::vector<std::string> files;
        files.clear();
        GetDirFiles(localConfig.path, files);
        std::vector<std::string> logs;
        logs.clear();
        for (auto &iter: files) {
            for (auto &suffix: localConfig.suffix) {
                if (iter.find(suffix) != std::string::npos) {
                    logs.push_back(iter);
                    break;
                }
            }
        }
        //删除文件
        time_t now;
        time(&now);
        struct stat buf;
        for (auto &iter: logs) {
            //文件的修改时间
            string fulPath = localConfig.path + "/" + iter;
            if (stat(fulPath.c_str(), &buf) == 0) {
                if ((now - buf.st_mtime) > localConfig.keepSeconds) {
                    std::cout << "准备删除文件 " << fulPath << std::endl;
                    if (remove(fulPath.c_str()) == 0) {
                        char bufInfo[512];
                        memset(bufInfo, 0, 512);
                        sprintf(bufInfo, "log file:%s modify time:%s,now:%s,keepSeconds:%d s,delete",
                                fulPath.c_str(), asctime(localtime(&buf.st_mtime)), asctime(localtime(&now)),
                                localConfig.keepSeconds);
                        std::cout << bufInfo << std::endl;
                    }
                }
            }
        }
    }
}
