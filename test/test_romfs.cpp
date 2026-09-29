//
// Created by lining on 2026/9/24.
//
#include <iostream>
#include "incbin.h"
#include "string"
#include "xpack/json.h"
#include "xpack/toml.h"
using namespace std;

INCBIN(my_data, PROJECT_ROOT_DIR"/romfs/test.toml");

typedef struct System {
    string name;
    int age;
    bool isFemale;
    XPACK(O(name,age,isFemale))
} System;

typedef struct Config {
    System system;
    XPACK(O(system))
} Config;


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

    Config c;
    xpack::toml::decode(toml_str, c);
    cout << "name:" << c.system.name << endl;
    cout << "age:" << c.system.age << endl;
    cout << "isFemale:" << c.system.isFemale << endl;

    auto json_str = R"({
                "system": {
                    "name": "test",
                    "age": 18,
                    "isFemale": true
                 }
                })";
    Config c1;
    xpack::json::decode(json_str, c1);
    cout << "name:" << c1.system.name << endl;
    cout << "age:" << c1.system.age << endl;
    cout << "isFemale:" << c1.system.isFemale << endl;

    return 0;
}
