//
// Created by lining on 2025/11/7.
//

#ifndef COMHANDLER_HPP
#define COMHANDLER_HPP
#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#endif
#include "common/common.h"
#include "common/proc.h"
#include "config/config.h"
#include "base/AsyncProc.hpp"
#include <glog/logging.h>
#include <thread>
#include <utility>
#include <vector>
#include <string>
#include <future>

using namespace std;

namespace common {
    class CommonHandler : public AsyncProc<string> {
    public:
        shared_future<int> future_t2;

    public:
        explicit CommonHandler(string name) : AsyncProc(std::move(name)) {
        }

        ~CommonHandler() override = default;

        void startBusiness() override {
            AsyncProc::startBusiness();
            future_t2 = std::async(std::launch::async, ThreadProcessPkg, this);
        }

        void stopBusiness() override {
            AsyncProc::stopBusiness();
            try {
                future_t2.wait();
            } catch (exception &e) {
                LOG(ERROR) << e.what();
            }
        }

    public:
        void Action() override {
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

        static int ThreadProcessPkg(CommonHandler *local) {
            LOG(WARNING) << local->_name << " ThreadProcessPkg start";
            while (local->_isRun) {
                Poco::AutoPtr<MsgNotification> pNf = dynamic_cast<MsgNotification *>(local->_pkgs.
                    waitDequeueNotification());
                if (pNf) {
                    string pkg = pNf->message();
                    pNf->release();
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
                            iter->second(local->_name, pkg);
                        } else {
                            LOG(ERROR) << local->_name << " 最后没有对应的方法名:" << code << ",内容:" << pkg;
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
            LOG(WARNING) << local->_name << " ThreadProcessPkg stop";
            return 0;
        }

    public:
        virtual int SendBase(string pkg) = 0;

        virtual int Send(char *buf_send, int len_send) = 0;
    };
}

#endif //COMHANDLER_HPP
