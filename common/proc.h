//
// Created by lining on 2023/4/23.
//

#ifndef PROC_H
#define PROC_H

#include <string>
#include <vector>
#include <mutex>
#include <common/common.h>

using namespace std;
using namespace common;

class CacheTimestamp {
private:
    std::vector<uint64_t> dataTimestamps;
    int dataIndex = -1;

public:
    std::mutex mtx;
    uint64_t interval = 0;
    bool isSetInterval = false;
    bool isStartTask = false;

public:
    void update(int index, uint64_t timestamp, int caches = 10);
};


namespace common {
    std::vector<std::string> split_path(const std::string &path);

    using HandleFun = std::function<std::string(const std::string &msgType, const std::string &guid, const std::string &req_json)>;

    extern map<vector<string>, HandleFun> HandleRouter;
}


#endif //PROC_H
