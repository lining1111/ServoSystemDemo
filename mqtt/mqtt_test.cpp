//
// Created by lining on 2026/7/29.
//
#include "mqtt/MQTT.h"

int main() {
    MQTT::Config c{
        .url = "tcp://192.168.225.90:1883",
        .clientID = "test",
        .topicSub = "topicSub",
        .topicPub = "topicPub",
        .qos = 1,
    };
    MQTT *mqtt = new MQTT(c);

    mqtt->init();
    mqtt->connect();
    while (1) {
        std::this_thread::sleep_for(1s);
        string msg;
        if (mqtt->_chan->receive(msg)) {
            std::cout << "receive msg: " << msg << std::endl;
        }
    }
}
