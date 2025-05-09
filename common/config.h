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
    typedef struct Config {
        vector<string> msgType;
        bool isCheckClient = false;
    XPACK(O(msgType, isCheckClient));
    } Config;
public:
    Config _config;

    LocalConfig();

    ~LocalConfig();

    bool isShowMsgType(const string &msgType);

    int getDeviceConfigFromYAML(const string &path);
};

extern LocalConfig localConfig;

class Device {
public:
    string g_user;//the user ip address need to reply
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
