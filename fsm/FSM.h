//
// Created by lining on 10/21/24.
//

#ifndef FSM_H
#define FSM_H

#include <mutex>
#include <Poco/NotificationQueue.h>

using namespace std;

class _RecvNotification : public Poco::Notification {
public:
    _RecvNotification(const bool isRecv) : _isRecv(isRecv) {}

    const bool isRecv() const { return _isRecv; }

private:
    bool _isRecv;
};


class FSM {
private:
    //环形buffer相关
    mutex *mtx = nullptr;
    uint8_t *buff = nullptr;

    size_t _capacity;
    int _read_pos;
    int _write_pos;
    int _available_for_read;
    int _available_for_write;
    //int available_for_write = rb_capacity - available_for_read;

    //状态机相关
    Poco::NotificationQueue _recv;
public:
    uint64_t _needLen = 1;//缓存需要有的数据长度
public:
    FSM(size_t capacity);

    ~FSM();

public:

    size_t Read(void *data, size_t count);

    size_t Write(const void *data, size_t count);

    int GetReadLen();

    int GetWriteLen();


    void Stop();

    void TriggerAction(char *data, int len);

    bool WaitTriggerAction();

    bool ShouldAction();

};


#endif //FSM_H
