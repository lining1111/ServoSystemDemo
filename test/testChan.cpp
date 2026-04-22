//
// Created by lining on 2026/4/22.
//

#include "utils/Chan.hpp"
// ------------------ 示例代码 ------------------
int main() {
    // 示例1：无缓冲 channel，同步通信
    Chan<int> unbuffered{0};
    std::thread sender([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        bool ok = unbuffered.send(42);
        std::cout << "发送 42 " << (ok ? "成功" : "失败") << std::endl;
    });
    int val;
    bool ok = unbuffered.receive(val);
    std::cout << "接收到的值: " << val << ", " << (ok ? "通道正常" : "通道已关闭") << std::endl;
    sender.join();

    // 示例2：有缓冲 channel，缓冲区大小3
    Chan<std::string> buffered{3};
    buffered.send("hello");
    buffered.send("world");
    buffered.send("!");
    // 缓冲区已满，再发会阻塞，这里用异步线程演示
    std::thread sender2([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        buffered.send("extra");
        std::cout << "发送 extra 成功" << std::endl;
    });
    std::string s;
    for (int i = 0; i < 4; ++i) {
        if (buffered.receive(s)) {
            std::cout << "接收到: " << s << std::endl;
        } else {
            std::cout << "通道已关闭" << std::endl;
            break;
        }
    }
    sender2.join();

    // 示例3：关闭 channel，接收剩余数据
    Chan<double> ch{2};
    ch.send(1.1);
    ch.send(2.2);
    ch.close();     // 关闭后不能再发送
    double d;
    while (ch.receive(d)) {
        std::cout << "关闭后接收: " << d << std::endl;
    }
    // 再次接收返回 false
    if (!ch.receive(d)) {
        std::cout << "通道已空且关闭，接收失败" << std::endl;
    }

    return 0;
}