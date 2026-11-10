//
// Created by lining on 2026/7/29.
//

#include "MQTT.h"

MQTT::MQTT(Config config) : _config(config) {
    //获取 mqtt 模块日志 handler
    log_ = LogHelper::Instance()->GetLogger("mqtt");
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
    //Chan 以 clientID 作为名称，便于监控时区分来源
    _chan = new Chan<std::string>(cacheSize, _config.clientID);
    _client = newClient(_config);
    _connectOptions.set_clean_session(true);
    _connectOptions.set_keep_alive_interval(20);
    _connectOptions.set_automatic_reconnect(true);
    _connectOptions.set_user_name(_config.username);
    _connectOptions.set_password(_config.password);
    _client->set_connected_handler([this](const string &cause) {
        LOG_TO(log_, spdlog::level::warn, "mqtt connect:{}", cause);
        isConnected = true;
        _client->subscribe(_config.topicSub, _config.qos);
    });
    _client->set_connection_lost_handler([this](const string &cause) {
        LOG_TO(log_, spdlog::level::err, "mqtt disconnect:{}", cause);
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
        LOG_TO(log_, spdlog::level::err, "first connect Error(ignore can be ok): {}", exc.what());
    }
}

bool MQTT::Pub(const std::string &msg) {
    if (this->_client == nullptr) {
        return false;
    }
    //未连接时不能发布
    if (!isConnected) {
        return false;
    }

    auto token = _client->publish(_config.topicPub, msg);
    return token->wait_for(std::chrono::seconds(10));
}
