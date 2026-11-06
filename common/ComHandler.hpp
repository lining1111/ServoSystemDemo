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
#include "logHelper/LogHelper.h"
#include <thread>
#include <utility>
#include <vector>
#include <string>
#include <future>

using namespace std;

namespace common {
    class CommonHandler : public AsyncProc<string> {
    public:
        std::thread t_procPkg;

    public:
        explicit CommonHandler(string name) : AsyncProc(std::move(name)) {
        }

        ~CommonHandler() override = default;

        void startBusiness() override {
            AsyncProc::startBusiness();
            t_procPkg = std::thread(ThreadProcessPkg, this);
        }

        void stopBusiness() override {
            AsyncProc::stopBusiness();
            if (t_procPkg.joinable()) {
                t_procPkg.join();
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
            LogHelper::Instance()->BizLog(spdlog::level::warn, "{} ThreadProcessPkg start", local->_name);
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
                            if (localConfig.isShowMsgType("COM")) {
                                LogHelper::Instance()->BizLog(spdlog::level::info, "local process:{}", pkg);
                            }
                            auto rsp_json = iter->second(code, guid, pkg);
                            local->SendBase(rsp_json);
                        } else {
                            LogHelper::Instance()->BizLog(spdlog::level::err, "{} 最后没有对应的方法名:{},内容:{}",
                                                          local->_name, code, pkg);
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
            LogHelper::Instance()->BizLog(spdlog::level::warn, "{} ThreadProcessPkg stop", local->_name);
            return 0;
        }

    public:
        virtual int SendBase(string pkg) = 0;

        virtual int Send(char *buf_send, int len_send) = 0;
    };
}

#endif //COMHANDLER_HPP
