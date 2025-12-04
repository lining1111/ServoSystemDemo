//
// Created by lining on 12/19/24.
//


#ifndef HTTPCLIENT_H
#define HTTPCLIENT_H

#include <string>
#include <map>

class HttpClient {
public:
    explicit HttpClient(const std::string &host, int port = 80);

    std::string Get(const std::string &path,
                    const std::map<std::string, std::string> &header,
                    int &state) const;

    std::string Post(const std::string &path,
                     const std::map<std::string, std::string> &header,
                     int &state) const;
private:
    std::string m_host;
    int m_port;
};

#endif // HTTPCLIENT_H


