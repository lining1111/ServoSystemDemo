//
// Created by lining on 12/19/24.
//

#include "HttpClient.h"
#include "httplib.h"

HttpClient::HttpClient(const std::string &host, int port)
        : m_host(host), m_port(port) {
}

std::string HttpClient::Get(const std::string &path,
                            const std::map<std::string, std::string> &header, int &state) {

    httplib::Client cli(m_host, m_port);
    httplib::Headers _headers;
    for (auto iter: header) {
        _headers.insert(iter);
    }

    auto res = cli.Get(path, _headers);
    state = res->status;

    return res->body;
}

std::string HttpClient::Post(const std::string &path,
                             const std::map<std::string, std::string> &header, int &state) {
    httplib::Client cli(m_host, m_port);
    httplib::Headers _headers;
    for (auto iter: header) {
        _headers.insert(iter);
    }

    auto res = cli.Post(path, _headers);
    state = res->status;

    return res->body;
}
