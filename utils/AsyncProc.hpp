//
// Created by lining on 2025/11/21.
//

#ifndef ASYNCPROC_HPP
#define ASYNCPROC_HPP
#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#endif


#include "logHelper/LogHelper.h"
#include "base/fsm/FSM.h"
#include <thread>
#include <vector>
#include <string>
#include <future>

class MsgNotification : public Poco::Notification {
public:
    explicit MsgNotification(const string &msg) : _msg(msg) {
    }

    string message() const { return _msg; }

private:
    string _msg;
};

template<typename T>
class AsyncProc {
public:
    std::mutex *mtx = nullptr;
    string _name;

    bool _isRun = false;

    int BUFFER_SIZE = 1024 * 1024 * 4;
    FSM *_fsm = nullptr;
    Poco::NotificationQueue _pkgs;
    int MaxQueueSize = 1024; //最大的消息队列长度
    T _pkgCache;
    std::thread t_stateMachine;
    uint64_t timeSend = 0;
    uint64_t timeRecv = 0;

public:
    explicit AsyncProc(string name = "notSet", int bufSize = 1024 * 1024 * 4,
                       int queueSize = 1024) : _name(std::move(name)),
                                               BUFFER_SIZE(bufSize), MaxQueueSize(queueSize) {
        timeRecv = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        timeSend = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        mtx = new std::mutex();
        _fsm = new FSM(BUFFER_SIZE);
    }

    virtual ~AsyncProc() {
        delete _fsm;
        delete mtx;
        BIZ_LOG(spdlog::level::warn, "{} release", _name);
    }

    virtual void startBusiness() {
        _isRun = true;
        BIZ_LOG(spdlog::level::warn, "{} start business", _name);
        t_stateMachine = std::thread(ThreadStateMachine, this);
    }

    virtual void stopBusiness() {
        _isRun = false;
        _fsm->Stop();
        _pkgs.wakeUpAll();
        if (t_stateMachine.joinable()) {
            t_stateMachine.join();
        }

        BIZ_LOG(spdlog::level::warn, "{} stop business", _name);
    }

    virtual void Action() = 0;

    // static int ThreadStateMachine(AsyncProc *local);//子类也要实现
private:
    static int ThreadStateMachine(AsyncProc *local) {
        BIZ_LOG(spdlog::level::warn, "{} {} start", local->_name, __FUNCTION__);
        local->_pkgCache.clear();
        while (local->_isRun) {
            if (local->_fsm->WaitTriggerAction()) {
                while (local->_fsm->ShouldAction()) {
                    local->Action();
                }
            }
        }
        BIZ_LOG(spdlog::level::warn, "{} {} stop", local->_name, __FUNCTION__);
        return 0;
    }
};


#endif //ASYNCPROC_HPP
