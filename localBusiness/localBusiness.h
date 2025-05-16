//
// Created by lining on 7/19/24.
//

#ifndef LOCALBUSINESS_H
#define LOCALBUSINESS_H

#include "utils/nonCopyable.hpp"
#include <map>
#include <string>
#include "Poco/Timer.h"
#include "myTcp/MyTcpClient.h"
#include "myTcp/MyTcpServer.h"
#include "myWebsocket/MyWebsocketServer.h"
#include "myWebsocket/MyWebsocketClient.h"

using namespace std;

class LocalBusiness : public NonCopyable {
public:
    static LocalBusiness *m_pInstance;

    bool isRun = false;
    std::map<string, MyTcpServer *> serverList;
    std::map<string, MyTcpClient *> clientList;
    std::map<string, MyWebsocketServer *> wsServerList;
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
        for (auto iter = wsClientList.begin(); iter != wsClientList.end();) {
            delete iter->second;
            iter = wsClientList.erase(iter);
        }
        for (auto iter = wsServerList.begin(); iter != wsServerList.end();) {
            delete iter->second;
            iter = wsServerList.erase(iter);
        }
    };

    void AddServer(const string &name, int port);

    void AddServer_ws(const string &name, int port);

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
     * 0 local tcp client
     * 1 remote tcp client
     * 2 local ws client
     * 3 remote ws client
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

    void ShowInfo();

public:
    Poco::Timer timerKeep;

private:
    void Task_Keep(Poco::Timer &timer);
};


#endif //LOCALBUSINESS_H
