//
// Created by lining on 2025/11/21.
//

#ifndef ASYNCPROC_HPP
#define ASYNCPROC_HPP
#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#endif

#include <glog/logging.h>
#include "base/fsm/FSM.h"
#include <thread>
#include <vector>
#include <string>
#include <future>

template<typename T>
class AsyncProc {
public:
    std::mutex *mtx = nullptr;
    string _name;

    bool _isRun = false;

    int _BUFFER_SIZE = 1024 * 1024 * 4;
    FSM *_fsm = nullptr;
    Poco::NotificationQueue _pkgs;
    int MaxQueueSize = 1024; //最大的消息队列长度
    T _pkgCache;
    bool isLocalThreadRun = false;
    shared_future<int> future_t1;
    // shared_future<int> future_t2;
    uint64_t timeSend = 0;
    uint64_t timeRecv = 0;

public:
    AsyncProc(string name = "notSet", int bufSize = 1024*1024*4, int queueSize = 1024) : _name(name), _BUFFER_SIZE(bufSize), MaxQueueSize(queueSize) {
        timeRecv = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        timeSend = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        mtx = new std::mutex();
        _fsm = new FSM(_BUFFER_SIZE);
    }

    virtual ~AsyncProc() {
        delete _fsm;
        delete mtx;
        LOG(WARNING) << _name << " release";
    }

    void startBusiness() {
        _isRun = true;
        LOG(WARNING) << _name << " start business";
        isLocalThreadRun = true;
        future_t1 = std::async(std::launch::async, ThreadStateMachine, this);
    }

    void stopBusiness() {
        _isRun = false;
        if (isLocalThreadRun) {
            isLocalThreadRun = false;
            _fsm->Stop();
            _pkgs.wakeUpAll();
        }
        LOG(WARNING) << _name << " stop business";
    }

    virtual void Action() = 0;

    // static int ThreadStateMachine(AsyncProc *local);//子类也要实现
private:
    static int ThreadStateMachine(AsyncProc *local) {
        LOG(WARNING) << local->_name << " ThreadStateMachine start";
        local->_pkgCache.clear();
        while (local->_isRun) {
            if (local->_fsm->WaitTriggerAction()) {
                while (local->_fsm->ShouldAction()) {
                    local->Action();
                }
            }
        }
        LOG(WARNING) << local->_name << " ThreadStateMachine end";
        return 0;
    }
};


#endif //ASYNCPROC_HPP
