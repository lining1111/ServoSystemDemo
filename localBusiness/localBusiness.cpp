//
// Created by lining on 7/19/24.
//

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#endif

#include "localBusiness.h"

LocalBusiness *LocalBusiness::m_pInstance = nullptr;

LocalBusiness *LocalBusiness::instance() {

    if (m_pInstance == nullptr) {
        m_pInstance = new LocalBusiness();
    }
    return m_pInstance;
}

void LocalBusiness::AddServer(const string &name, int port) {
    auto server = new MyTcpServer(port);
    serverList.insert(make_pair(name, server));
}

void LocalBusiness::AddServer_ws(const string &name, int port) {
    auto server = new MyWebsocketServer(port);
    wsServerList.insert(make_pair(name, server));
}

void LocalBusiness::AddClient(const string &name, const string &cloudIp, int cloudPort) {
    auto client = new MyTcpClient(cloudIp, cloudPort);//端口号和ip依实际情况而变
    clientList.insert(make_pair(name, client));
}

void LocalBusiness::AddClient_ws(const string &name, const string &cloudIp, int cloudPort) {
    auto client = new MyWebsocketClient(cloudIp, cloudPort);//端口号和ip依实际情况而变
    wsClientList.insert(make_pair(name, client));
}

void LocalBusiness::Run() {
    isRun = true;
    for (auto &iter: serverList) {
        auto s = iter.second;
        if (s->Open() == 0) {
        }
        s->Run();
        std::this_thread::sleep_for(std::chrono::milliseconds(3 * 1000));
    }

    for (auto &iter: wsServerList) {
        auto s = iter.second;
        if (s->Open() == 0) {
            s->Run();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(3 * 1000));
    }

    for (auto &iter: clientList) {
        auto client = iter.second;
        if (client->Open() == 0) {
        }
        client->Run();
        std::this_thread::sleep_for(std::chrono::milliseconds(3 * 1000));
    }

    for (auto &iter: wsClientList) {
        auto client = iter.second;
        if (client->Open() == 0) {
        }
        client->Run();
        std::this_thread::sleep_for(std::chrono::milliseconds(3 * 1000));
    }

    StartTimerTask();
}

void LocalBusiness::Stop() {
    StopTimerTaskAll();
    isRun = false;
    for (auto &iter: serverList) {
        auto s = iter.second;
        delete s;
    }
    for (auto &iter: clientList) {
        auto c = iter.second;
        delete c;
    }

    for (auto &iter: wsClientList) {
        auto c = iter.second;
        delete c;
    }
}

void LocalBusiness::addConn(MyTcpServerHandler *p) {
    std::unique_lock<std::mutex> lock(mtx);
    _conns.push_back(p);
}

void LocalBusiness::addConn_ws(MyWebSocketRequestHandler *p) {
    std::unique_lock<std::mutex> lock(mtx_ws);
    _conns_ws.push_back(p);
}

void LocalBusiness::delConn(const string &peerAddress) {
    std::unique_lock<std::mutex> lock(mtx);
    for (int i = 0; i < _conns.size(); i++) {
        if (_conns.at(i) != nullptr) {
            auto c = _conns.at(i);
            if (c->_peerAddress == peerAddress) {
                _conns.erase(_conns.begin() + i);
                LOG(WARNING) << "从数组踢出客户端:" << peerAddress;
            }
        }
    }
}

void LocalBusiness::delConn_ws(const string &peerAddress) {
    std::unique_lock<std::mutex> lock(mtx_ws);
    for (int i = 0; i < _conns_ws.size(); i++) {
        if (_conns_ws.at(i) != nullptr) {
            auto c = _conns_ws.at(i);
            if (c->_peerAddress == peerAddress) {
                _conns_ws.erase(_conns_ws.begin() + i);
                LOG(WARNING) << "从ws数组踢出客户端:" << peerAddress;
            }
        }
    }
}

void LocalBusiness::stopAllConns() {
    LOG(WARNING) << "stop allConns";
    std::unique_lock<std::mutex> lock(mtx);
    for (int i = 0; i < _conns.size(); i++) {
        if (_conns.at(i) != nullptr) {
            auto c = _conns.at(i);
            _conns.erase(_conns.begin() + i);
            LOG(WARNING) << "从数组踢出客户端:" << c->_peerAddress;
            c->stopBusiness();
            delete c;
        }
    }
}

void LocalBusiness::stopAllConns_ws() {
    std::unique_lock<std::mutex> lock(mtx_ws);
    for (int i = 0; i < _conns_ws.size(); i++) {
        if (_conns_ws.at(i) != nullptr) {
            auto c = _conns_ws.at(i);
            _conns_ws.erase(_conns_ws.begin() + i);
            LOG(WARNING) << "从ws数组踢出客户端:" << c->_peerAddress;
            delete c;
        }
    }
}

void LocalBusiness::Broadcast(const string &msg) {
    std::unique_lock<std::mutex> lock(mtx);
    for (auto iter: _conns) {
        if (iter != nullptr) {
            iter->SendBase(msg);
        }
    }

    std::unique_lock<std::mutex> lock_ws(mtx_ws);
    for (auto iter: _conns_ws) {
        if (iter != nullptr) {
            iter->SendBase(msg);
        }
    }

    if (!clientList.empty()) {
        for (auto iter: clientList) {
            auto c = iter.second;
            if (!c->isNeedReconnect) {
                c->SendBase(msg);
            }
        }
    }

    if (!wsClientList.empty()) {
        for (auto iter: wsClientList) {
            auto c = iter.second;
            if (!c->isNeedReconnect) {
                c->SendBase(msg);
            }
        }
    }
}

int LocalBusiness::SendToClient(const string &peerAddress, const string &msg) {
    int ret = -1;
    bool isRemoteClient = false;
    //先看下是不是对端的客户端
    std::unique_lock<std::mutex> lock(mtx);
    for (auto iter: _conns) {
        if (iter != nullptr) {
            if (iter->_peerAddress == peerAddress) {
                ret = iter->SendBase(msg);
                isRemoteClient = true;
            }
        }
    }

    std::unique_lock<std::mutex> lock_ws(mtx_ws);
    for (auto iter: _conns_ws) {
        if (iter != nullptr) {
            if (iter->_peerAddress == peerAddress) {
                ret = iter->SendBase(msg);
                isRemoteClient = true;
            }
        }
    }

    //如果不是对端的客户端，大概是本地的客户端
    if (!isRemoteClient) {
        if (!clientList.empty()) {
            for (auto iter: clientList) {
                auto c = iter.second;
                if (c->_peerAddress == peerAddress && !c->isNeedReconnect) {
                    ret = c->SendBase(msg);
                }
            }
        }

        if (!wsClientList.empty()) {
            for (auto iter: wsClientList) {
                auto c = iter.second;
                if (c->_peerAddress == peerAddress && !c->isNeedReconnect) {
                    ret = c->SendBase(msg);
                }
            }
        }
    }

    return ret;
}

void *LocalBusiness::FindClient(const string &peerAddress, CLIType &clientType) {
    void *ret = nullptr;
    bool isRemoteClient = false;
    //先看下是不是对端的客户端
    std::unique_lock<std::mutex> lock(mtx);
    for (auto iter: _conns) {
        if (iter != nullptr) {
            if (iter->_peerAddress == peerAddress) {
                ret = iter;
                clientType = CT_REMOTETCP;
                isRemoteClient = true;
            }
        }
    }

    std::unique_lock<std::mutex> lock_ws(mtx_ws);
    for (auto iter: _conns_ws) {
        if (iter != nullptr) {
            if (iter->_peerAddress == peerAddress) {
                ret = iter;
                clientType = CT_REMOTEWS;
                isRemoteClient = true;
            }
        }
    }

    //如果不是对端的客户端，大概是本地的客户端
    if (!isRemoteClient) {
        if (!clientList.empty()) {
            for (auto iter: clientList) {
                auto c = iter.second;
                if (c->_peerAddress == peerAddress) {
                    ret = iter.second;
                    clientType = CT_LOCALTCP;
                }
            }
        }
        if (!wsClientList.empty()) {
            for (auto iter: wsClientList) {
                auto c = iter.second;
                if (c->_peerAddress == peerAddress) {
                    ret = iter.second;
                    clientType = CT_LOCALWS;
                }
            }
        }
    }

    return ret;
}

void LocalBusiness::kickoff(uint64_t timeout, uint64_t now) {
    std::unique_lock<std::mutex> lock(mtx);
    for (int i = 0; i < _conns.size(); i++) {
        auto conn = _conns.at(i);
        if (conn != nullptr) {
            if (abs((long long) now - (long long) conn->timeRecv) > timeout) {
                LOG(WARNING) << "接收超时,主动断开客户端:" << conn->_peerAddress;
                delete conn;
                _conns.erase(_conns.begin() + i);
            }
        }
    }

    std::unique_lock<std::mutex> lock_ws(mtx_ws);
    for (int i = 0; i < _conns_ws.size(); i++) {
        auto conn = _conns_ws.at(i);
        if (conn != nullptr) {
            if (abs((long long) now - (long long) conn->timeRecv) > timeout) {
                LOG(WARNING) << "接收超时,主动断开客户端:" << conn->_peerAddress;
                delete conn;
                _conns_ws.erase(_conns_ws.begin() + i);
            }
        }
    }
}


void LocalBusiness::StartTimerTask() {
    timerKeep.start(1000 * 3, std::bind(Task_Keep, this));
}

void LocalBusiness::StopTimerTaskAll() {
    timerKeep.stop();
}

void LocalBusiness::ShowInfo() {
    //local tcp
    LOG(WARNING) << "local tcp info";
    if (serverList.empty()) {
        LOG(WARNING) << "no local server start";
    } else {
        for (auto s: serverList) {
            LOG(WARNING) << "local server: " << s.first << "---" << s.second->_s.address().toString();
            if (_conns.empty()) {
                LOG(WARNING) << "no remote client connect";
            } else {
                std::unique_lock<std::mutex> lock(mtx);
                for (auto c: _conns) {
                    LOG(WARNING) << "remote client: " << c->_peerAddress;
                }
            }
        }
        if (clientList.empty()) {
            LOG(WARNING) << "no local client start";
        } else {
            for (auto c: clientList) {
                LOG(WARNING) << "local client: " << c.first << "---" << c.second->_peerAddress;
            }
        }

        //local ws
        LOG(WARNING) << "local ws info";
        if (wsServerList.empty()) {
            LOG(WARNING) << "no local ws server start";
        } else {
            for (auto s: wsServerList) {
                LOG(WARNING) << "local ws server: " << s.first << "---127.0.0.1:" << s.second->_port;
                if (_conns_ws.empty()) {
                    LOG(WARNING) << "no remote ws client connect";
                } else {
                    std::unique_lock<std::mutex> lock(mtx_ws);
                    for (auto c: _conns_ws) {
                        LOG(WARNING) << "remote client: " << c->_peerAddress;
                    }
                }
            }
        }
        if (wsClientList.empty()) {
            LOG(WARNING) << "no local ws client start";
        } else {
            for (auto c: wsClientList) {
                LOG(WARNING) << "local ws client: " << c.first << "---" << c.second->_peerAddress;
            }
        }
    }
}

    void LocalBusiness::Task_Keep(void *p) {
        if (p == nullptr) {
            return;
        }
        auto local = (LocalBusiness *) p;

        if (local->serverList.empty() && local->clientList.empty()
            && local->wsServerList.empty() && local->wsClientList.empty()) {
            return;
        }

        if (local->isRun) {
            for (auto &iter: local->serverList) {
                auto s = iter.second;
                if (!s->isListen) {
                    s->ReOpen();
                    if (s->isListen) {
                        LOG(WARNING) << "服务端:" << iter.first << " port:" << s->_port
                                     << " 重启";
                    } else {
                        LOG(WARNING) << "服务端:" << iter.first << " port:" << s->_port
                                     << " 重启失败";
                    }
                }
            }

            for (auto &iter: local->wsServerList) {
                auto s = iter.second;
                if (!s->isListen) {
                    s->ReOpen();
                    if (s->isListen) {
                        s->Run();
                        LOG(WARNING) << "服务端:" << iter.first << " port:" << s->_port
                                     << " 重启";
                    } else {
                        LOG(WARNING) << "服务端:" << iter.first << " port:" << s->_port
                                     << " 重启失败";
                    }
                }
            }

            for (auto &iter: local->clientList) {
                auto c = iter.second;
                if (c->isNeedReconnect) {
                    c->Reconnect();
                    if (!c->isNeedReconnect) {
                        LOG(WARNING) << "客户端:" << iter.first << " " << c->server_ip << "_" << c->server_port
                                     << " 重启";
                    } else {
                        LOG(WARNING) << "客户端:" << iter.first << " " << c->server_ip << "_" << c->server_port
                                     << " 重启失败";
                    }
                }
            }

            for (auto &iter: local->wsClientList) {
                auto c = iter.second;
                if (c->isNeedReconnect) {
                    c->Reconnect();
                    if (!c->isNeedReconnect) {
                        LOG(WARNING) << "客户端:" << iter.first << " " << c->server_ip << "_" << c->server_port
                                     << " 重启";
                    } else {
                        LOG(WARNING) << "客户端:" << iter.first << " " << c->server_ip << "_" << c->server_port
                                     << " 重启失败";
                    }
                }
            }
        }
    }


