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

    //完整报文处理：分包后的报文(不含包尾'*')路由到 HandleRouter，返回应答报文(不含包尾'*')
    //作为 tcp::Server / tcp::Client / websocket::Server / websocket::Client 的报文处理回调使用
    std::string ProcessPkg(const std::string &pkg);
}


#endif //PROC_H
