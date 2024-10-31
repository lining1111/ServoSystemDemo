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

using namespace std;

//配置类
class LocalConfig {
public:
    typedef struct Config {
        vector<string> msgType;
        bool isCheckClient = false;
    } Config;
public:
    Config _config;

    LocalConfig();

    ~LocalConfig();

    bool isShowMsgType(const string& msgType);

    int getDeviceConfigFromINI(const string& iniPath);
};

extern LocalConfig localConfig;

class Device {
public:
    string  g_user;//需要回复的客户端地址
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
