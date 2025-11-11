//
// Created by lining on 2025/11/7.
//

#ifndef SERVOSYSTEMDEMO_COMHANDLER_H
#define SERVOSYSTEMDEMO_COMHANDLER_H
#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#endif
#include "common/common.h"
#include "common/proc.h"
#include "config/config.h"
#include "fsm/FSM.h"
#include <glog/logging.h>
#include <thread>
#include <vector>
#include <string>
#include <future>

using namespace std;

namespace common {
    class CommonHandler {
    public:
        std::mutex *mtx = nullptr;
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
        CommonHandler() {
            timeRecv = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            timeSend = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            mtx = new std::mutex();
            _fsm = new FSM(BUFFER_SIZE);
        }

        virtual ~CommonHandler() {
            delete _fsm;
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


        static int ThreadStateMachine(CommonHandler *local) {
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


        static int ThreadProcessPkg(CommonHandler *local) {
            LOG(WARNING) << local->_peerAddress << " ThreadProcessPkg start";
            while (local->_isRun) {
                Poco::AutoPtr<MsgNotification> pNf = dynamic_cast<MsgNotification *>(local->_pkgs.
                    waitDequeueNotification());
                if (pNf) {
                    string pkg = pNf->message();
                    if (pkg.empty()) {
                        continue;
                    }
                    string guid, code, param;
                    tie(guid, code, param) = parseCom(pkg);

                    if (guid.empty()) {
                        guid = getGuid();
                    }

                    if (!code.empty()) {
                        auto keys = common::split_path(code);
                        auto iter = HandleRouter.find(keys);
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
        virtual int SendBase(string pkg) = 0;

        virtual int Send(char *buf_send, int len_send) = 0;
    };
}

#endif //SERVOSYSTEMDEMO_COMHANDLER_H
