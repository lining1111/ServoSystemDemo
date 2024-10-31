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

int LocalConfig::getDeviceConfigFromINI(const string& iniPath) {
    //判断文件是否存在
    if (!std::ifstream(iniPath)) {
        LOG(ERROR) << "ini file not exist";
        return -1;
    }

    mINI::INIFile file(iniPath);
    mINI::INIStructure iniStructure;
    if (file.read(iniStructure)) {
        {
            string msgTypeArray = iniStructure["msgType"]["msg"];
            _config.msgType = StringSplit(msgTypeArray, ",");
        }
        {
            if (iniStructure["isCheckClient"]["check"] != "0") {
                _config.isCheckClient = true;
            }
        }
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