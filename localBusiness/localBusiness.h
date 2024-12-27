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
#include "myWebsocket/MyWebsocketServerHandler.h"
#include "myWebsocket/MyWebsocketClient.h"

using namespace std;

class LocalBusiness : public NonCopyable {
public:
    static LocalBusiness *m_pInstance;

    bool isRun = false;
    std::map<string, MyTcpServer *> serverList;
    std::map<string, MyTcpClient *> clientList;
    std::map<string, MyWebsocketClient *> wsClientList;
private:
    mutex mtx;
    vector<MyTcpServerHandler *> _conns;//接入的客户端数组

    mutex mtx_ws;
    vector<MyWebSocketRequestHandler *> _conns_ws;//接入的客户端数组

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

    void AddClient_ws(const string &name, const string &cloudIp, int cloudPort);

    void Run();

    void Stop();

    void addConn(MyTcpServerHandler *p);

    void addConn_ws(MyWebSocketRequestHandler *p);

    void delConn(const string &peerAddress);

    void delConn_ws(const string &peerAddress);

    void stopAllConns();

    void stopAllConns_ws();

    void Broadcast(const string &msg);

    int SendToClient(const string &peerAddress, const string &msg);

    /*
     * clientType
     * 0 本地tcp客户端
     * 1 远端tcp客户端
     * 2 本地ws客户端
     * 3 远端ws客户端
     */

    enum CLIType {
        CT_LOCALTCP = 0,
        CT_REMOTETCP = 1,
        CT_LOCALWS = 2,
        CT_REMOTEWS = 3,
    };

    void *FindClient(const string &peerAddress, CLIType &clientType);

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
