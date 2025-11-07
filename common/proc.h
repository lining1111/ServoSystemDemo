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
    typedef int (*Handle)(const string &h, const string &content);

    extern map<string, Handle> HandleRouter;
}



#endif //PROC_H
