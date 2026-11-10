//
// Created by lining on 2023/4/23.
//

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#endif

#include "proc.h"
#include "common.h"
#include "config/config.h"
#include "logHelper/LogHelper.h"
#include <fmt/core.h>
#include <fmt/ranges.h>
#include "utils/utils.h"
#include <bitset>
#include <utility>


void CacheTimestamp::update(int index, uint64_t timestamp, int caches) {
    std::unique_lock<std::mutex> lock(mtx);
    //If it has already been updated, do not proceed with the following operations.
    if (!isSetInterval) {
        //Check if index is set; if not, the default value is -1.
        if (dataIndex == -1) {
            dataIndex = index;
            dataTimestamps.clear();
            dataTimestamps.push_back(timestamp);
        } else {
            //Check if it is the corresponding index.
            if (dataIndex == index) {
                //Check if the timestamp is increasing; if not, clear the previous content and start over.
                if (dataTimestamps.empty()) {
                    //if queue is empty push it
                    dataTimestamps.push_back(timestamp);
                } else {
                    if (timestamp <= dataTimestamps.back()) {
                        BIZ_LOG(spdlog::level::err,
                                                      "当前插入时间戳 {} 小于已插入的最新时间戳 {}，将恢复到最初状态",
                                                      timestamp, dataTimestamps.back());
                        dataTimestamps.clear();
                        dataIndex = -1;
                    } else {
                        //If it is the corresponding index, store the timestamp in the queue.
                        dataTimestamps.push_back(timestamp);
                    }
                }
            }
        }
        //After the cache frame is full, calculate the frame rate and set the flag.
        if (dataTimestamps.size() == caches) {
            BIZ_LOG(spdlog::level::warn, "动态帧率原始时间戳队列:{}",
                                          fmt::format("{}", dataTimestamps));
            std::vector<uint64_t> intervals;
            for (int i = 1; i < dataTimestamps.size(); i++) {
                auto cha = dataTimestamps.at(i) - dataTimestamps.at(i - 1);
                intervals.push_back(cha);
            }
            //calculate the average frame rate
            uint64_t sum = 0;
            for (auto iter: intervals) {
                sum += iter;
            }
            this->interval = sum / intervals.size();
            //            printf("帧率的差和为%d\n", sum);
            //            printf("计算的帧率为%d\n", this->interval);
            this->isSetInterval = true;
        }
    }
}

#include <Poco/StringTokenizer.h>

namespace common {
    std::vector<std::string> split_path(const std::string &path) {
        Poco::StringTokenizer tokenizer(path, "/", Poco::StringTokenizer::TOK_IGNORE_EMPTY);
        return std::vector<std::string>(tokenizer.begin(), tokenizer.end());
    }

    std::string ProcessPkg(const std::string &pkg) {
        if (pkg.empty()) {
            return "";
        }
        std::string guid, code, param;
        std::tie(guid, code, param) = parseCom(pkg);

        if (guid.empty()) {
            guid = getGuid();
        }

        Com rsp;
        if (!code.empty()) {
            auto keys = split_path(code);
            auto iter = HandleRouter.find(keys);
            if (iter != HandleRouter.end()) {
                if (localConfig.isShowMsgType("COM")) {
                    BIZ_LOG(spdlog::level::info, "local process:{}", pkg);
                }
                return iter->second(code, guid, pkg);
            }
            BIZ_LOG(spdlog::level::err, "最后没有对应的方法名:{},内容:{}", code, pkg);
            rsp.guid = guid;
            rsp.code = code;
            rsp.state = State_UnmarshalFail;
            rsp.param = "code not find:" + code;
        } else {
            rsp.guid = guid;
            rsp.state = State_UnmarshalFail;
            rsp.param = "code empty";
        }
        return json::encode(rsp);
    }

    //处理handle的基础模板类
    template<typename TReq, typename TRsp>
    class HandlerBase {
    public:
        virtual ~HandlerBase() = default;

        // 统一处理入口
        std::string Process(const std::string &code, const std::string &guid, const std::string &req_json) {
            BIZ_LOG(spdlog::level::info, "Process code: {}, guid: {}, req_json: {}",
                                          code, guid, req_json);
            TReq req;
            // 1. 请求解码 —— 失败直接返回错误回复
            try {
                xpack::json::decode(req_json, req);
            } catch (const std::exception &e) {
                return MakeError(code, guid, -100, std::string("decode failed: ") + e.what());
            }

            // 2. 业务处理 —— 失败也返回错误回复
            TRsp rsp;
            try {
                rsp = HandleBusiness(req);
            } catch (const std::exception &e) {
                return MakeError(code, guid, -101, std::string("business failed: ") + e.what());
            }

            // 3. 回复编码
            try {
                return xpack::json::encode(rsp);
            } catch (const std::exception &e) {
                return MakeError(code, guid, -102, std::string("encode failed: ") + e.what());
            }
        }

    protected:
        virtual TRsp HandleBusiness(const TReq &req) = 0;

    private:
        // 统一错误回复编码，编码本身也需保护
        static std::string MakeError(const string &code, const string &guid, const int state,
                                     const std::string &info) {
            Com err;
            err.code = code;
            err.guid = guid;
            err.state = state;
            err.param = info;
            return xpack::json::encode(err);
        }
    };

    class HeartbeatHandler : public HandlerBase<Com, Com> {
    protected:
        Com HandleBusiness(const Com &req) override {
            Com rsp;
            rsp.code = req.code;
            rsp.guid = req.guid;
            rsp.state = 0;
            rsp.timestamp = req.timestamp;
            rsp.param = "ok";
            return rsp;
        }
    };


    map<vector<string>, HandleFun> HandleRouter = {
        {
            {"Heartbeat"}, [](const string &code, const string &guid, const string &req_json) {
                HeartbeatHandler handler;
                return handler.Process(code, guid, req_json);
            }
        },

    };
}
