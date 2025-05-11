//
// Created by lining on 7/16/24.
//

#ifndef UTILS_H
#define UTILS_H

#include <cstdint>
#include <string>
#include <vector>
#include <codecvt>
#include <locale>

using namespace std;

//大小端转换
uint16_t swap_uint16(uint16_t val);

uint32_t swap_uint32(uint32_t val);


int runCmd(const std::string &command, std::string *output = nullptr,
           bool redirect_stderr = false);

uint64_t getTimestampMs();

string getGuid();

// 取文件夹名字 无后缀
string getFolderPath(const string &str);

// 取后缀
string getFileSuffix(const string &str);

// 取文件名字 不包括后缀
string getFileName(const string &str);

// 去掉后缀
string getRemoveSuffix(const string &str);

// 取文件名字 包括后缀
string getFileNameAll(const string &str);

int GetVectorFromFile(vector<uint8_t> &array, const string &filePath);

int GetFileFromVector(vector<uint8_t> &array, const string &filePath);

vector<string> StringSplit(const string &in, const string &delim);

/**
 * 打印hex输出
 * @param data
 * @param len
 */
void PrintHex(uint8_t *data, uint32_t len);

class chs_codecvt : public codecvt_byname<wchar_t, char, std::mbstate_t> {
public:
    chs_codecvt(string s) : codecvt_byname(s) {

    }
};

string Utf8ToGbk(const string &str);

string GbkToUtf8(const string &str);

wstring Utf8ToUnicode(const string &str);

wstring GbkToUnicode(const string &str);

string UnicodeToUtf8(const wstring &str);

string UnicodeToGbk(const wstring &str);


void Trim(string &str, char trim);

bool startsWith(const std::string str, const std::string prefix);

bool endsWith(const std::string str, const std::string suffix);

void
base64_encode(unsigned char *input, unsigned int input_length, unsigned char *output, unsigned int *output_length);

void
base64_decode(unsigned char *input, unsigned int input_length, unsigned char *output, unsigned int *output_length);

string getIpStr(unsigned int ip);

bool isIPv4(string IP);

bool isIPv6(string IP);

string validIPAddress(string IP);

void GetDirFiles(const string &path, vector<string> &array);


//创建路径文件夹
void CreatePath(const std::string &path);

struct MemoryInfo {
    uint64_t total;     // 总内存(bytes)
    uint64_t available; // 可用内存(bytes)
    uint64_t used;      // 已用内存(bytes)
    double usage;       // 内存使用率(0-1)
};

bool GetMemoryInfo(MemoryInfo &info);

struct DiskSpaceInfo {
    uint64_t total;     // 总空间大小（字节）
    uint64_t free;      // 剩余空间大小（字节）
    uint64_t available; // 可用空间大小（字节）
};

bool GetCurrentDirectorySpace(DiskSpaceInfo &info);

struct NetworkInfo {
    bool isUp = false;// 网络是否连接
    string name; // 网络接口名称
    string ip;   // IP地址
    string mask; // 子网掩码
    string mac;  // MAC地址
};

void getAllNetworkInterfaces(vector<NetworkInfo> &networks);

bool isProcessRunning(string processName);

//计算函数执行耗时
template<typename Func, typename... Args>
auto measureExecutionTime(Func &&func, Args &&... args, double &elapsedTime) -> decltype(auto);

#endif //UTILS_H
