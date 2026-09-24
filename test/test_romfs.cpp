//
// Created by lining on 2026/9/24.
//
#include <iostream>
#include "incbin.h"
#include "string"
#include "toml++/toml.hpp"
using namespace std;

INCBIN(my_data, PROJECT_ROOT_DIR"/romfs/test.toml");

int main() {
    const string toml_str(reinterpret_cast<const char *>(gmy_dataData), gmy_dataSize);
    try {
        auto config = toml::parse(toml_str);
        const auto name = config["system"]["name"].value_or("unknown");
        cout << "name:" << name << endl;
    } catch (const toml::parse_error &err) {
        std::cerr << "解析失败: " << err.description() << "\n";
        return 1;
    }

    return 0;
}
