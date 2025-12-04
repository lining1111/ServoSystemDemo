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
        LocalServerConfig localTcpServerConfig;
        RemoteServerConfig remoteTcpServerConfig;
        //WS
        LocalServerConfig localWSServerConfig;
        RemoteServerConfig remoteWSServerConfig;
        XPACK(O(msgType, localTcpServerConfig, remoteTcpServerConfig, localWSServerConfig, remoteWSServerConfig));
    } Config;

public:
    Config _config;

    LocalConfig();

    ~LocalConfig();

    bool isShowMsgType(const string &msgType) const;

    int getDeviceConfigFromYAML(const string &path);
};

extern LocalConfig localConfig;

class Device {
public:
    string g_user; //the user ip address need to reply
public:
    Device();

    ~Device();

public:
    void Init();

    void Keep();

    void Exit();
};

extern Device device;

#endif //CONFIG_H
