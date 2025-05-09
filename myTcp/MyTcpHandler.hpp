//
// Created by lining on 2023/8/22.
//

#ifndef MYTCPHANDLER_H
#define MYTCPHANDLER_H

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#endif
#include "common/common.h"
#include "common/CRC.h"
#include "common/proc.h"
#include "common/config.h"
#include "fsm/FSM.h"
#include <glog/logging.h>
#include <thread>
#include <vector>
#include <string>
#include <future>
#include "Poco/Net/StreamSocket.h"

using namespace std;

using Poco::Net::StreamSocket;


class MyTcpHandler {
public:
    std::mutex *mtx = nullptr;
    StreamSocket _socket;
    std::string _peerAddress;

    bool _isRun = false;

    int BUFFER_SIZE = 1024 * 1024 * 4;
    FSM *_fsm = nullptr;
    Poco::NotificationQueue _pkgs;
    string _pkgCache;

    bool isLocalThreadRun = false;
    shared_future<int> future_t1;
    shared_future<int> future_t2;
    uint64_t timeSend = 0;
    uint64_t timeRecv = 0;

public:
    MyTcpHandler() {
        if (mtx == nullptr) {
            mtx = new std::mutex();
        }
        _fsm = new FSM(BUFFER_SIZE);
    }

    ~MyTcpHandler() {
        if (_fsm != nullptr) {
            delete _fsm;
            _fsm = nullptr;
        }
        delete mtx;
        LOG(WARNING) << _peerAddress << " release";
    }

    void startBusiness() {
        _isRun = true;
        LOG(WARNING) << _peerAddress << " start business";
        isLocalThreadRun = true;
        future_t1 = std::async(std::launch::async, ThreadStateMachine, this);
        future_t2 = std::async(std::launch::async, ThreadProcessPkg, this);

    }

    void stopBusiness() {
        _isRun = false;
        if (isLocalThreadRun) {
            isLocalThreadRun = false;
            _fsm->Stop();
            _pkgs.wakeUpAll();
            try {
                future_t1.wait();
            } catch (exception &e) {
                LOG(ERROR) << e.what();
            }
            try {
                future_t2.wait();
            } catch (exception &e) {
                LOG(ERROR) << e.what();
            }
        }
        LOG(WARNING) << _peerAddress << " stop business";
    }

private:

    void Action() {
        char value = 0x00;
        if (_fsm->Read(&value, 1) == 1) {
            if (value == '*') {
                //得到分包尾部
                _pkgs.enqueueNotification(Poco::AutoPtr<MsgNotification>(new MsgNotification(_pkgCache)));
                _pkgCache.clear();
            } else {
                _pkgCache.push_back(value);
            }
        }
    }


    static int ThreadStateMachine(MyTcpHandler *local) {
        LOG(WARNING) << local->_peerAddress << " ThreadStateMachine start";
        local->_pkgCache.clear();
        while (local->_isRun) {
            if (local->_fsm->WaitTriggerAction()) {
                while (local->_fsm->ShouldAction()) {
                    local->Action();
                }
            }

        }
        LOG(WARNING) << local->_peerAddress << " ThreadStateMachine end";
        return 0;
    }


    static int ThreadProcessPkg(MyTcpHandler *local) {
        LOG(WARNING) << local->_peerAddress << " ThreadProcessPkg start";
        while (local->_isRun) {
            Poco::AutoPtr<MsgNotification> pNf = dynamic_cast<MsgNotification *>(local->_pkgs.waitDequeueNotification());
            if (pNf) {
                string pkg = pNf->message();
                if (pkg.empty()) {
                    continue;
                }
                auto guid = common::parseGUID(pkg);
                if (guid.empty()) {
                    guid = getGuid();
                }

                auto code = common::parseCode(pkg);
                if (!code.empty()) {
                    auto iter = HandleRouter.find(code);
                    if (iter != HandleRouter.end()) {
                        LOG_IF(INFO, localConfig.isShowMsgType("COM")) << "local process:" << pkg;
                        iter->second(local->_peerAddress, pkg);
                    } else {
                        LOG(ERROR) << local->_peerAddress << " 最后没有对应的方法名:" << code << ",内容:" << pkg;
                        Com rsp;
                        rsp.guid = guid;
                        rsp.code = code;
                        rsp.state = State_UnmarshalFail;
                        rsp.param = "code not find:" + code;
                        string rspStr = json::encode(rsp);
                        local->SendBase(rspStr);
                    }
                } else {
                    Com rsp;
                    rsp.guid = guid;
                    rsp.code = "";
                    rsp.state = State_UnmarshalFail;
                    rsp.param = "code empty";
                    string rspStr = json::encode(rsp);
                    local->SendBase(rspStr);
                }
            }
        }
        LOG(WARNING) << local->_peerAddress << " ThreadProcessPkg end";
        return 0;
    }


public:

    int SendBase(string pkg) {
        int ret = 0;
        std::unique_lock<std::mutex> lock(*mtx);
        if (pkg.back() != '*') {
            pkg.push_back('*');
        }
        LOG_IF(INFO, localConfig.isShowMsgType("COM")) << "Rsp:" << pkg;
        try {
            auto len = _socket.sendBytes(pkg.data(), pkg.length());
            VLOG(2) << _peerAddress << " send len:" << len << " len_send:" << to_string(pkg.length());
            if (len < 0) {
                LOG(ERROR) << _peerAddress << " send len < 0";
                ret = -2;
            } else if (len != pkg.size()) {
                LOG(ERROR) << _peerAddress << " send len !=len_send";
                ret = -2;
            }
        }
        catch (Poco::Exception &exc) {
            LOG(ERROR) << _peerAddress << " send error:" << exc.code() << exc.displayText();
            if (exc.code() != POCO_ETIMEDOUT && exc.code() != POCO_EWOULDBLOCK && exc.code() != POCO_EAGAIN) {
                ret = -2;
            } else {
                ret = -3;
            }
        }

        if (this->timeSend == 0) {
            this->timeRecv = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
        }

        this->timeSend = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

        return ret;
    }

    int Send(char *buf_send, int len_send) {
        int ret = 0;
        std::unique_lock<std::mutex> lock(*mtx);
        LOG_IF(INFO, localConfig.isShowMsgType("COM")) << "Rsp:" << string(buf_send);
        try {
            auto len = _socket.sendBytes(buf_send, len_send);
            VLOG(2) << _peerAddress << " send len:" << len << " len_send:" << len_send;
            if (len < 0) {
                LOG(ERROR) << _peerAddress << " send len < 0";
                ret = -2;
            } else if (len != len_send) {
                LOG(ERROR) << _peerAddress << " send len !=len_send";
                ret = -2;
            }
        }
        catch (Poco::Exception &exc) {
            LOG(ERROR) << _peerAddress << " send error:" << exc.code() << exc.displayText();
            if (exc.code() != POCO_ETIMEDOUT && exc.code() != POCO_EWOULDBLOCK && exc.code() != POCO_EAGAIN) {
                ret = -2;
            } else {
                ret = -3;
            }
        }

        if (this->timeSend == 0) {
            this->timeRecv = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
        }

        this->timeSend = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

        return ret;
    }

};


#endif //MYTCPHANDLER_H
