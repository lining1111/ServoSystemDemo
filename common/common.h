//
// Created by lining on 2/16/22.
//

#ifndef _COMMON_H
#define _COMMON_H

#include <cstdint>
#include <string>
#include <vector>
#include <glog/logging.h>
#include "utils/utils.h"
#include <Poco/NotificationQueue.h>

using namespace std;

#include <xpack/json.h>

using namespace xpack;

namespace common {

#define ARRAY_SIZE(x) \
    (sizeof(x)/sizeof(x[0]))
#define OFFSET(type, member)      \
    ((size_t)(&(((type*)0)->member)))
#define MEMBER_SIZE(type, member) \
    sizeof(((type *)0)->member)
#define STR(p) p?p:""

    class MsgNotification : public Poco::Notification {
    public:
        MsgNotification(const string msg) : _msg(msg) {}

        const string message() const { return _msg; }

    private:
        string _msg;
    };


#define ComVersion "0.0.1"

    string GetComVersion();

    class Com {
    public:
        string comVersion = ComVersion;
        string guid;
        uint64_t timestamp;
        string code;//命令码
        int state = 0;
        string param;
    XPACK(O(comVersion, guid, timestamp, code, state, param));

        Com() {
            guid = getGuid();
            timestamp = getTimestampMs();
        }
    };

    
    string parseCode(const string &pkg);

    string parseGUID(const string &pkg);

    /*
     * 回复data中state枚举
     * 0：成功 其他，失败，具体情况按指来定，
     * 每次的通信都是以Req开始，Rsp结束，涉及注册、验证需要在交互期间发送提示信息的，提示信息类型以Note表示
     *
     */
    enum State {
        State_NoClient = -1000,//未有客户端连接
        State_ParamErr = -102,//获取参数失败
        State_NotFindCmd = -101,//未找到cmd
        State_UnmarshalFail = -100,//协议反序列化失败
        State_CmdExeNoRsp = 100,//此状态下，执行完不用回复客户端
        State_Success = 0,//成功
        State_Null = -1,//模块为null,未初始化
        State_Unconnect = -2,//未连接
        State_SendErr = -3,//发送失败
        State_LongTimeNoRecv = -4,//长时间未收到模块信息

    };


}


#endif //_COMMON_H
