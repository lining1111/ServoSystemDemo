//
// Created by lining on 2026/7/29.
//

#include "MQTT.h"

MQTT::MQTT(Config config) : _config(config) {
}

MQTT::~MQTT() {
    if (isConnected) {
        _client->disconnect();
    }
}

mqtt::async_client *MQTT::newClient(Config config) {
    return new mqtt::async_client(config.url, config.clientID);
}

void MQTT::init(int cacheSize) {
    _chan = new Chan<std::string>(cacheSize);
    _client = newClient(_config);
    _connectOptions.set_clean_session(true);
    _connectOptions.set_keep_alive_interval(20);
    _connectOptions.set_automatic_reconnect(true);
    _connectOptions.set_user_name(_config.username);
    _connectOptions.set_password(_config.password);
    _client->set_connected_handler([this](const string &cause) {
        std::cout << "mqtt connect:" << cause << std::endl;
        isConnected = true;
        _client->subscribe(_config.topicSub, _config.qos);
    });
    _client->set_connection_lost_handler([this](const string &cause) {
        std::cout << "mqtt disconnect" << std::endl;
        isConnected = false;
    });

    _client->set_message_callback([this](mqtt::const_message_ptr msg) {
        _chan->send(msg->get_payload_str());
    });
}

void MQTT::connect() {
    try {
        _client->connect(_connectOptions)->wait();
        _client->start_consuming();
    } catch (const mqtt::exception &exc) {
        std::cerr << " first connect Error(ignore can be ok): " << exc.what() << std::endl;
    }
}

bool MQTT::Pub(const std::string &msg) {
    if (this->_client == nullptr) {
        return false;
    }
    if (isConnected) {
        return false;
    }

    auto token = _client->publish(_config.topicPub, msg);
    token->wait_for(std::chrono::seconds(10));
}
