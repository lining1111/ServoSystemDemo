//
// Created by lining on 2023/4/23.
//
#define _HAS_STD_BYTE 0
#include "config.h"
#include "utils//utils.h"
#include <fstream>
#include "logHelper/LogHelper.h"
#include <Poco/Util/Application.h>
#include <Poco/AutoPtr.h>
#include <Poco/Util/IniFileConfiguration.h>
#include <mini/ini.h>

using namespace Poco::Util;
using Poco::AutoPtr;
using Poco::Util::IniFileConfiguration;

LocalConfig::LocalConfig() = default;

LocalConfig::~LocalConfig() = default;

bool LocalConfig::isShowMsgType(const string& msgType) const {
    if (_config.msgType.empty()) {
        return false;
    } else {
        bool isExist = false;
        for (auto iter: _config.msgType) {
            if (iter == msgType) {
                isExist = true;
                break;
            }
        }
        return isExist;
    }
}

int LocalConfig::getDeviceConfigFromYAML(const string& path) {
    //判断文件是否存在
    if (!std::ifstream(path)) {
        BIZ_LOG(spdlog::level::err, "config file not exist");
        return -1;
    }

    try {
        yaml::decode_file(path, this->_config);
    } catch (exception &e) {
        BIZ_LOG(spdlog::level::err, "{}", e.what());
        return -1;
    }


    return 0;
}

LocalConfig localConfig;
