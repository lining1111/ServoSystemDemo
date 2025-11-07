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
        explicit MsgNotification(const string msg) : _msg(msg) {
        }

        string message() const { return _msg; }

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
        string code;
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
     * Reply frame data state enumeration
     * 0：success，other:fail
     * Each communication starts with “Req” and ends with “Rsp”.
     * Note types are used to send prompt information during interactions involving registration and verification.
     */
    enum State {
        State_NoClient = -1000, //no client connected
        State_ParamErr = -102, //get param error
        State_NotFindCmd = -101, //not find cmd
        State_UnmarshalFail = -100, //unmarshal fail
        State_CmdExeNoRsp = 100,
        //in this case, the command is executed successfully, but the reply frame is not returned
        State_Success = 0, //success
        State_Null = -1, //module info is null,not init
        State_Unconnect = -2, //unconnect
        State_SendErr = -3, //send error
        State_LongTimeNoRecv = -4, //long time no recv
    };
}


#endif //_COMMON_H
