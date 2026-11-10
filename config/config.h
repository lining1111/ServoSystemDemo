//
// Created by lining on 2023/4/23.
//

#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <vector>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>
#include "xpack/yaml.h"

using namespace std;
using namespace xpack;

class LocalConfig {
public:
    typedef struct LocalServerConfig {
        bool isUse = false;
        string name;
        int port = 8083;
        bool isCheckClient = false;
        int timeout = 30*1000;
        XPACK(O(isUse, name, port, isCheckClient, timeout));
    } LocalServerConfig;

    typedef struct RemoteServerConfig {
        bool isUse = false;
        string name;
        string ip;
        int port = 8083;
        XPACK(O(isUse, name, ip, port));
    } RemoteServerConfig;

    typedef struct Config {
        vector<string> msgType;
        //TCP
        LocalServerConfig localTcpServer;
        RemoteServerConfig remoteTcpServer;
        //WS
        LocalServerConfig localWSServer;
        RemoteServerConfig remoteWSServer;
        //字段名与 config.yaml 的 key 保持一致（xpack 按成员名匹配 yaml key，不匹配会静默跳过）
        XPACK(O(msgType, localTcpServer, remoteTcpServer, localWSServer, remoteWSServer));
    } Config;

public:
    Config _config;

    LocalConfig();

    ~LocalConfig();

    bool isShowMsgType(const string &msgType) const;

    int getDeviceConfigFromYAML(const string &path);
};

extern LocalConfig localConfig;


#endif //CONFIG_H
