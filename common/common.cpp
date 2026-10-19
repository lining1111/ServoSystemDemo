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

    tuple<string, string, string> parseCom(const string &pkg) {
        Com data;
        try {
            json::decode(pkg, data);
        } catch (std::exception &e) {
            LOG(ERROR) << e.what();
            return tie("", "", "");
        }
        return tie(data.guid, data.code, data.param);
    }

}
