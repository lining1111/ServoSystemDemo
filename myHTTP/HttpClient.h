//
// Created by lining on 12/19/24.
//


#ifndef HTTPCLIENT_H
#define HTTPCLIENT_H

#include <string>
#include <map>
#include <Poco/URI.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPCredentials.h>
#include <Poco/Net/HTMLForm.h>
#include <Poco/StreamCopier.h>
#include <Poco/NullStream.h>
#include <Poco/Exception.h>

class HttpClient {
public:
    HttpClient(const std::string &host, const std::string &port = "80");

    std::string Get(const std::string &path,
                    const std::map<std::string, std::string> &header,
                    int &state);

    // 在Query中添加表单数据
    std::string Post(const std::string &path,
                     const std::map<std::string, std::string> &query,
                     const std::map<std::string, std::string> &header,
                     int &state);

    // 在Body中使用JSON或XML数据
    std::string Post(const std::string &path,
                     const std::string &body,
                     const std::map<std::string, std::string> &header,
                     int &state);

    bool DoRequest(Poco::Net::HTTPClientSession &session,
                   Poco::Net::HTTPRequest &request,
                   Poco::Net::HTTPResponse &response,
                   std::string &responseMsg, int &state);

    bool DoRequest(Poco::Net::HTTPClientSession &session,
                   Poco::Net::HTTPRequest &request,
                   Poco::Net::HTTPResponse &response,
                   Poco::Net::HTMLForm &Query,
                   std::string &responseMsg, int &state);

    bool DoRequest(Poco::Net::HTTPClientSession &session,
                   Poco::Net::HTTPRequest &request,
                   Poco::Net::HTTPResponse &response,
                   const std::string &Body,
                   std::string &responseMsg, int &state);

private:
    std::string m_host;
    std::string m_port;
};

#endif // HTTPCLIENT_H


