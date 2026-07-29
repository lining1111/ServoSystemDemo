//
// Created by lining on 2026/7/29.
//

#ifndef MQTT_H
#define MQTT_H
#include <atomic>
#include <string>
#include "mqtt/async_client.h"
#include "utils/Chan.hpp"

using namespace std;

class MQTT {
public:
    typedef struct Config {
        std::string url;
        string clientID;
        string username;
        string password;
        string topicSub;
        string topicPub;
        int qos = 0;
    } Config;

    mqtt::async_client *_client;
    Chan<std::string> *_chan;

private:
    mqtt::connect_options _connectOptions{};
    bool _isRun = false;
    atomic<bool> isConnected{false};
    Config _config;

public:
    explicit MQTT(Config config);

    virtual ~MQTT();

    static mqtt::async_client *newClient(Config config);

    void init(int cacheSize = 10);

    void connect();

    bool Pub(const std::string &msg);

    // virtual void ThreadProcRecv() = 0;
};


#endif //MQTT_H
