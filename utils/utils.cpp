//
// Created by lining on 7/16/24.
//

#include "utils.h"
#include <cstring>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cerrno>
#include <iostream>

#if defined(__linux__)

#include <unistd.h>
#include <sys/resource.h>
#include <sys/times.h>
#include <sys/sysinfo.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>

#elif defined(WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#pragma comment(lib, "pdh.lib")
#endif

#include <chrono>
#include <Poco/File.h>
#include <Poco/Path.h>
#include "Poco/UUID.h"
#include "Poco/UUIDGenerator.h"
#include <Poco/Process.h>
#include <Poco/PipeStream.h>
#include <Poco/StreamCopier.h>
#include <Poco/Net/NetworkInterface.h>
#include <Poco/DirectoryIterator.h>

uint16_t swap_uint16(uint16_t val) {
    return ((val & 0xff00) >> 8) | ((val & 0x00ff) << 8);
}

uint32_t swap_uint32(uint32_t val) {
    return ((val & 0xff000000) >> 24) | ((val & 0x00ff0000) >> 8) | ((val & 0x0000ff00) << 8) |
           ((val & 0x000000ff) << 24);
}


int runCmd(const std::string &command, std::string *output, bool redirect_stderr) {
    const auto &cmd = redirect_stderr ? command + " 2>&1" : command;
#if defined(__linux__)
    auto pipe = popen(cmd.c_str(), "r");
#elif defined(WIN32)
    auto pipe = _popen(cmd.c_str(), "r");
#endif
    if (!pipe) {
        //记录日志
        return errno == 0 ? -1 : errno;
    }
    {
        char buffer[1024] = {0};
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            if (output) {
                output->append(buffer);
            }
//            printf("%s",buffer);
        }
    }
#if defined(__linux__)
    return pclose(pipe);
#elif defined(_WIN32)
    return _pclose(pipe);
#endif
}

uint64_t getTimestampMs() {
//    return std::chrono::duration_cast<std::chrono::milliseconds>(
//            std::chrono::system_clock::now().time_since_epoch()).count();
    return Poco::Timestamp().epochMicroseconds() / 1000;
}

string getGuid() {
    Poco::UUID uuid = Poco::UUIDGenerator::defaultGenerator().createRandom();
    return uuid.toString();
}


// 取文件夹名字 无后缀
string getFolderPath(const string &str) {
    string::size_type idx = str.rfind('/', str.length());
    string folder = str.substr(0, idx);
    return folder;
}

// 取后缀
string getFileSuffix(const string &str) {
    string::size_type idx = str.rfind('.', str.length());
    string suffix = str.substr(idx + 1, str.length());
    return suffix;
}

// 取文件名字 不包括后缀
string getFileName(const string &str) {
    string::size_type idx = str.rfind('/', str.length());
    string::size_type pidx = str.rfind('.', str.length());
    string filename = str.substr(idx + 1, pidx - (idx + 1));
    return filename;
}

// 去掉后缀
string getRemoveSuffix(const string &str) {
    string::size_type idx = str.rfind('.', str.length());
    string filename = str.substr(0, idx);
    return filename;
}

// 取文件名字 包括后缀
string getFileNameAll(const string &str) {
    string::size_type idx = str.rfind('/', str.length());
    string name_all = str.substr(idx + 1, str.length());
    return name_all;
}

int GetVectorFromFile(vector<uint8_t> &array, const string &filePath) {
    ifstream fout;
    fout.open(filePath.c_str(), ios::in | ios::binary);
    if (fout.is_open()) {
        char val;
        while (fout.get(val)) {
            array.push_back(val);
        }
        fout.seekg(0, ios::beg);
        fout.close();
    } else {
        return -1;
    }

    return 0;
}

int GetFileFromVector(vector<uint8_t> &array, const string &filePath) {
    fstream fin;
    fin.open(filePath.c_str(), ios::out | ios::binary | ios::trunc);
    if (fin.is_open()) {
        for (auto iter: array) {
            fin.put(iter);
        }
        fin.flush();
        fin.close();
    } else {
        return -1;
    }

    return 0;
}

vector<string> StringSplit(const string &in, const string &delim) {
    stringstream tran(in.c_str());
    string tmp;
    vector<string> out;
    out.clear();
    while (std::getline(tran, tmp, *(delim.c_str()))) {
        out.push_back(tmp);
    }
    return out;
}

void PrintHex(uint8_t *data, uint32_t len) {
    int count = 0;
    for (int i = 0; i < len; i++) {
        printf("%02x ", data[i]);
        count++;
        if (count == 16) {
            printf("\n");
            count = 0;
        }
    }
    printf("\n");
}

wstring GbkToUnicode(const string &str) {
//    codecvt_byname<wchar_t, char, mbstate_t>*dd=;
    wstring_convert<chs_codecvt> gbk(new chs_codecvt("zh_CN.GBK"));    //GBK - whar

    return gbk.from_bytes(str);
}

string GbkToUtf8(const string &str) {
    return UnicodeToUtf8(GbkToUnicode(str));
}

string Utf8ToGbk(const string &str) {
    return UnicodeToGbk(Utf8ToUnicode(str));

}

wstring Utf8ToUnicode(const string &str) {
    wstring ret;

    wstring_convert<codecvt_utf8<wchar_t>> wcv;
    ret = wcv.from_bytes(str);
    return ret;
}

string UnicodeToUtf8(const wstring &wstr) {
    string ret;
    wstring_convert<codecvt_utf8<wchar_t>> wcv;
    ret = wcv.to_bytes(wstr);
    return ret;
}

string UnicodeToGbk(const wstring &wstr) {
    wstring_convert<chs_codecvt> gbk(new chs_codecvt("zh_CN.GBK"));    //GBK - whar
    string ret = gbk.to_bytes(wstr);
    return ret;

}

void Trim(string &str, char trim) {
    std::string::iterator end_pos = std::remove(str.begin(), str.end(), trim);
    str.erase(end_pos, str.end());
}

bool startsWith(const std::string str, const std::string prefix) {
    return (str.rfind(prefix, 0) == 0);
}

bool endsWith(const std::string str, const std::string suffix) {
    if (suffix.length() > str.length()) {
        return false;
    }

    return (str.rfind(suffix) == (str.length() - suffix.length()));
}

const char encodeCharacterTable[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
const signed char decodeCharacterTable[256] = {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
        52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
        -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
        41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

void
base64_encode(unsigned char *input, unsigned int input_length, unsigned char *output, unsigned int *output_length) {
    char buff1[3];
    char buff2[4];
    unsigned char i = 0, j;
    unsigned int input_cnt = 0;
    unsigned int output_cnt = 0;

    while (input_cnt < input_length) {
        buff1[i++] = input[input_cnt++];
        if (i == 3) {
            output[output_cnt++] = encodeCharacterTable[(buff1[0] & 0xfc) >> 2];
            output[output_cnt++] = encodeCharacterTable[((buff1[0] & 0x03) << 4) + ((buff1[1] & 0xf0) >> 4)];
            output[output_cnt++] = encodeCharacterTable[((buff1[1] & 0x0f) << 2) + ((buff1[2] & 0xc0) >> 6)];
            output[output_cnt++] = encodeCharacterTable[buff1[2] & 0x3f];
            i = 0;
        }
    }
    if (i) {
        for (j = i; j < 3; j++) {
            buff1[j] = '\0';
        }
        buff2[0] = (buff1[0] & 0xfc) >> 2;
        buff2[1] = ((buff1[0] & 0x03) << 4) + ((buff1[1] & 0xf0) >> 4);
        buff2[2] = ((buff1[1] & 0x0f) << 2) + ((buff1[2] & 0xc0) >> 6);
        buff2[3] = buff1[2] & 0x3f;
        for (j = 0; j < (i + 1); j++) {
            output[output_cnt++] = encodeCharacterTable[buff2[j]];
        }
        while (i++ < 3) {
            output[output_cnt++] = '=';
        }
    }
    output[output_cnt] = 0;
    *output_length = output_cnt;
}

void
base64_decode(unsigned char *input, unsigned int input_length, unsigned char *output, unsigned int *output_length) {
    char buff1[4];
    char buff2[4];
    unsigned char i = 0, j;
    unsigned int input_cnt = 0;
    unsigned int output_cnt = 0;

    while (input_cnt < input_length) {
        buff2[i] = input[input_cnt++];
        if (buff2[i] == '=') {
            break;
        }
        if (++i == 4) {
            for (i = 0; i != 4; i++) {
                buff2[i] = decodeCharacterTable[buff2[i]];
            }
            output[output_cnt++] = (char) ((buff2[0] << 2) + ((buff2[1] & 0x30) >> 4));
            output[output_cnt++] = (char) (((buff2[1] & 0xf) << 4) + ((buff2[2] & 0x3c) >> 2));
            output[output_cnt++] = (char) (((buff2[2] & 0x3) << 6) + buff2[3]);
            i = 0;
        }
    }
    if (i) {
        for (j = i; j < 4; j++) {
            buff2[j] = '\0';
        }
        for (j = 0; j < 4; j++) {
            buff2[j] = decodeCharacterTable[buff2[j]];
        }
        buff1[0] = (buff2[0] << 2) + ((buff2[1] & 0x30) >> 4);
        buff1[1] = ((buff2[1] & 0xf) << 4) + ((buff2[2] & 0x3c) >> 2);
        buff1[2] = ((buff2[2] & 0x3) << 6) + buff2[3];
        for (j = 0; j < (i - 1); j++) {
            output[output_cnt++] = (char) buff1[j];
        }
    }
//        output[output_cnt] = 0;
    *output_length = output_cnt;
}


string getIpStr(unsigned int ip) {
    union IPNode {
        unsigned int addr;
        struct {
            unsigned char s1;
            unsigned char s2;
            unsigned char s3;
            unsigned char s4;
        };
    };
    union IPNode x;

    x.addr = ip;
    char buffer[64];
    memset(buffer, 0, 64);
    sprintf(buffer, "%d.%d.%d.%d", x.s1, x.s2, x.s3, x.s4);

    return string(buffer);
}

bool isIPv4(string IP) {
    int dotcnt = 0;
    //How many '.' are there in total
    for (int i = 0; i < IP.length(); i++) {
        if (IP[i] == '.')
            dotcnt++;
    }
    //ipv4 address must have 3 '.'
    if (dotcnt != 3)
        return false;
    string temp = "";
    for (int i = 0; i < IP.length(); i++) {
        if (IP[i] != '.')
            temp += IP[i];
        //each segment must be 1-3 digits 0-255
        if (IP[i] == '.' || i == IP.length() - 1) {
            if (temp.length() == 0 || temp.length() > 3)
                return false;
            for (int j = 0; j < temp.length(); j++) {
                if (!isdigit(temp[j]))
                    return false;
            }
            int tempInt = stoi(temp);
            if (tempInt > 255 || tempInt < 0)
                return false;
            string convertString = to_string(tempInt);
            if (convertString != temp)
                return false;
            temp = "";
        }
    }
    if (IP[IP.length() - 1] == '.')
        return false;
    return true;
}

bool isIPv6(string IP) {
    int dotcnt = 0;
    for (int i = 0; i < IP.length(); i++) {
        if (IP[i] == ':')
            dotcnt++;
    }
    if (dotcnt != 7) return false;
    string temp = "";
    for (int i = 0; i < IP.length(); i++) {
        if (IP[i] != ':')
            temp += IP[i];
        if (IP[i] == ':' || i == IP.length() - 1) {
            if (temp.length() == 0 || temp.length() > 4)
                return false;
            for (int j = 0; j < temp.length(); j++) {
                if (!(isdigit(temp[j]) || (temp[j] >= 'a' && temp[j] <= 'f') || (temp[j] >= 'A' && temp[j] <= 'F')))
                    return false;
            }
            temp = "";
        }
    }
    if (IP[IP.length() - 1] == ':')
        return false;
    return true;
}


string validIPAddress(string IP) {
    //以.和：来区分ipv4和ipv6
    for (int i = 0; i < IP.length(); i++) {
        if (IP[i] == '.')
            return isIPv4(IP) ? "IPv4" : "Neither";
        else if (IP[i] == ':')
            return isIPv6(IP) ? "IPv6" : "Neither";
    }
    return "Neither";
}


void GetDirFiles(const string &path, vector<string> &array) {
    try {
        Poco::DirectoryIterator it(path);
        Poco::DirectoryIterator end;
        while (it != end) {
            if (it->isFile()) // 判断是文件还是子目录
            {
                array.push_back(it.name());
//                std::cout << it.path().toString() << std::endl;        //获取当前文件的的绝对路径名，it.name()只表示文件名
            }
//            else {
////                std::cout << "DirectoryName: " << it.path().toString() << std::endl;    //输出当前目录的绝对路径名（包含文件夹的名字）
//                GetDirFiles(it.path().toString(), array);
//            }
            ++it;
        }
    }
    catch (Poco::Exception &exc) {
//        std::cerr << exc.displayText() << std::endl;

    }
}


void CreatePath(const std::string &path) {
    Poco::Path dirPath(path);
    Poco::File dir(dirPath);
    if (!dir.exists()) {
        dir.createDirectories(); // 递归创建多级目录
        std::cout << "Created directory: " << dirPath.toString() << std::endl;
    } else {
        std::cout << "Directory already exists." << std::endl;
    }
}

bool GetMemoryInfo(MemoryInfo &info) {
#if defined(_WIN32)
    // Windows实现
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(memStatus);
    if (!GlobalMemoryStatusEx(&memStatus)) {
        return false;
    }

    info.total = memStatus.ullTotalPhys;
    info.available = memStatus.ullAvailPhys;
    info.used = info.total - info.available;
    info.usage = static_cast<double>(info.used) / info.total;

#elif defined(__linux__)
    // Linux实现
    struct sysinfo sysInfo;
    if (sysinfo(&sysInfo) != 0) {
        return false;
    }

    info.total = sysInfo.totalram * sysInfo.mem_unit;
    info.available = sysInfo.freeram * sysInfo.mem_unit;
    info.used = info.total - info.available;
    info.usage = static_cast<double>(info.used) / info.total;
#else
    // 不支持的操作系统
    return false;
#endif

    return true;
}

bool GetCurrentDirectorySpace(DiskSpaceInfo &info) {
#if defined(_WIN32) || defined(_WIN64)
    // Windows实现
    ULARGE_INTEGER totalBytes, freeBytes, availableBytes;

    // 获取当前工作目录所在的驱动器
    char currentDir[MAX_PATH];
    if (!GetCurrentDirectoryA(MAX_PATH, currentDir)) {
        return false;
    }

    // 提取驱动器根路径（如"C:\"）
    std::string rootPath = currentDir;
    if (rootPath.size() >= 2 && rootPath[1] == ':') {
        rootPath = rootPath.substr(0, 3); // 转换为"C:\"形式
    } else {
        // 可能是UNC路径或其他格式
        return false;
    }

    if (GetDiskFreeSpaceExA(rootPath.c_str(), &availableBytes, &totalBytes, &freeBytes)) {
        info.total = totalBytes.QuadPart;
        info.free = freeBytes.QuadPart;
        info.available = availableBytes.QuadPart;
        return true;
    }
    return false;
#else
    // Unix/Linux/macOS实现
    char currentDir[PATH_MAX];
    if (!getcwd(currentDir, sizeof(currentDir))) {
        return false;
    }

    struct statvfs vfs;
    if (statvfs(currentDir, &vfs) != 0) {
        return false;
    }

    // 计算空间大小
    info.total = vfs.f_blocks * vfs.f_frsize;
    info.free = vfs.f_bfree * vfs.f_frsize;
    info.available = vfs.f_bavail * vfs.f_frsize;
    return true;
#endif
}

void getAllNetworkInterfaces(vector<NetworkInfo> &networks) {
    Poco::Net::NetworkInterface::Map interfaces = Poco::Net::NetworkInterface::map();

    for (const auto &iface: interfaces) {
        std::cout << "Interface: " << iface.second.name() << std::endl;
        std::cout << "  IP Address: " << iface.second.address().toString() << std::endl;
        std::cout << "  Subnet Mask: " << iface.second.subnetMask().toString() << std::endl;
        std::cout << "  MAC Address: " << iface.second.macAddress() << std::endl;
        std::cout << "--------------------------------" << std::endl;
        NetworkInfo item;
        item.isUp = iface.second.isUp();
        item.name = iface.second.name();
        item.ip = iface.second.address().toString();
        item.mask = iface.second.subnetMask().toString();
        item.mac.assign(iface.second.macAddress().begin(), iface.second.macAddress().end());
    }
}


bool isProcessRunning(string processName) {
    try {
        // 构造 pgrep 命令
        std::vector<std::string> args = {processName};
        Poco::Pipe outPipe;
        Poco::ProcessHandle ph = Poco::Process::launch(
                "pgrep",
                args,
                0,  // stdin
                &outPipe,  // stdout
                0   // stderr
        );

        // 读取命令输出
        Poco::PipeInputStream istr(outPipe);
        std::string output;
        Poco::StreamCopier::copyToString(istr, output);

        // 如果 pgrep 返回非空，说明进程存在
        return !output.empty();
    } catch (const Poco::Exception &e) {
        std::cerr << "Error executing pgrep: " << e.displayText() << std::endl;
        return false;
    }
}


template<typename Func, typename... Args>
auto measureExecutionTime(Func &&func, Args &&... args, double &elapsedTime) -> decltype(auto) {
    // 使用std::chrono::high_resolution_clock获取高精度时间
    auto start = std::chrono::high_resolution_clock::now();    // 调用函数并捕获其返回值
    auto result = func(std::forward<Args>(args)...);    // 获取结束时间
    auto end = std::chrono::high_resolution_clock::now();    // 计算持续时间并转换为微秒

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    elapsedTime = static_cast<double>(duration.count()); // 转换为double
    // 返回函数的执行结果
    return result;
}
