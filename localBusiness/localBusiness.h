//
// Created by lining on 7/19/24.
//

#ifndef LOCALBUSINESS_H
#define LOCALBUSINESS_H

#include "utils/nonCopyable.hpp"
#include <map>
#include <string>
#include "utils//timeTask.hpp"
#include "myTcp/MyTcpClient.h"
#include "myTcp/MyTcpServer.h"

using namespace std;

class LocalBusiness : public NonCopyable {
public:
    static LocalBusiness *m_pInstance;

    bool isRun = false;
    std::map<string, MyTcpServer *> serverList;
    std::map<string, MyTcpClient *> clientList;
private:
    mutex mtx;
    vector<MyTcpServerHandler *> _conns;//接入的客户端数组

public:
    static LocalBusiness *instance();

    ~LocalBusiness() {
        StopTimerTaskAll();
        isRun = false;
        for (auto iter = clientList.begin(); iter != clientList.end();) {
            delete iter->second;
            iter = clientList.erase(iter);
        }
        for (auto iter = serverList.begin(); iter != serverList.end();) {
            delete iter->second;
            iter = serverList.erase(iter);
        }
    };

    void AddServer(const string &name, int port);

    void AddClient(const string &name, const string &cloudIp, int cloudPort);

    void Run();

    void Stop();

    void addConn(MyTcpServerHandler *p);

    void delConn(const string& peerAddress);

    void stopAllConns();

    void Broadcast(const string& msg);

    int SendToClient(const string& peerAddress, const string& msg);

    void *FindClient(const string& peerAddress);

    void kickoff(uint64_t timeout, uint64_t now);


public:
    void StartTimerTask();

    void StopTimerTaskAll();

public:
    utils::Timer timerKeep;

private:
    /**
    * 查看服务端和客户端状态，并执行断线重连
    * @param p
    */
    static void Task_Keep(void *p);
};


#endif //LOCALBUSINESS_H
