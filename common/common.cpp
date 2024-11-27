//
// Created by lining on 2/16/22.
//

#include <iostream>
#include "common.h"
#include "utils/utils.h"

namespace common {

    string GetComVersion() {
        return ComVersion;
    }

    string parseCode(const string &pkg) {
        Com data;
        data.code.clear();
        try {
            json::decode(pkg, data);
        } catch (std::exception &e) {
            LOG(ERROR) << e.what();
            return "";
        }
        return data.code;
    }

    string parseGUID(const string &pkg) {
        Com data;
        data.guid.clear();
        try {
            json::decode(pkg, data);
        } catch (std::exception &e) {
            LOG(ERROR) << e.what();
            return "";
        }
        return data.guid;
    }


}