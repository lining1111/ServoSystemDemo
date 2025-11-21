//
// Created by lining on 2023/4/23.
//

#include "config.h"
#include "utils//utils.h"
#include <fstream>
#include <glog/logging.h>
#include <Poco/Util/Application.h>
#include <Poco/Path.h>
#include <Poco/AutoPtr.h>
#include <Poco/Util/IniFileConfiguration.h>
#include <mini/ini.h>

using namespace Poco::Util;
using Poco::AutoPtr;
using Poco::Util::IniFileConfiguration;

LocalConfig::LocalConfig() {

}

LocalConfig::~LocalConfig() {

}

bool LocalConfig::isShowMsgType(const string& msgType) {
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
        LOG(ERROR) << "config file not exist";
        return -1;
    }

    try {
        yaml::decode_file(path, this->_config);
    } catch (exception &e) {
        LOG(ERROR) << e.what();
        return -1;
    }


    return 0;
}

LocalConfig localConfig;

Device::Device() {

}

Device::~Device() {

}

void Device::Init() {
    LOG(WARNING) << "Device init";
    //TODO device init
}

void Device::Keep() {
    //TODO device keep
}

void Device::Exit() {
    LOG(WARNING) << "Device exit";
   //TODO device exit
}

Device device;