//
// Created by lining on 12/19/24.
//

#include "HttpClient.h"
#include <sstream>

HttpClient::HttpClient(const std::string &host, const std::string &port)
        : m_host(host), m_port(port) {
}

std::string HttpClient::Get(const std::string &path,
                            const std::map<std::string, std::string> &header, int &state) {
    try {
        Poco::Net::HTTPClientSession session(m_host, std::stoi(m_port));
        Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, path, Poco::Net::HTTPMessage::HTTP_1_1);
        // 设置请求头
        request.set("User-Agent", "PocoRuntime-PocoRuntime/1.1.0");
        for (auto item: header) {
            request.set(item.first, item.second);
        }

        Poco::Net::HTTPResponse response;

        std::string retMsg;
        if (DoRequest(session, request, response, retMsg, state)) {
            printf("%s\n", retMsg.c_str());
        }
        return retMsg;
    }
    catch (const Poco::Exception &ex) {
        printf("%s\n", ex.displayText().c_str());
    }

    return "";
}

std::string HttpClient::Post(const std::string &path,
                             const std::map<std::string, std::string> &query,
                             const std::map<std::string, std::string> &header, int &state) {
    try {
        Poco::Net::HTTPClientSession session(m_host, std::stoi(m_port));
        Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_POST, path, Poco::Net::HTTPMessage::HTTP_1_1);
        // 设置请求头
        request.set("User-Agent", "PocoRuntime-PocoRuntime/1.1.0");
        for (auto item: header) {
            request.set(item.first, item.second);
        }

        // 设置请求参数
        Poco::Net::HTMLForm formQuery;
        for (auto item: query) {
            formQuery.add(item.first, item.second);
        }
        formQuery.prepareSubmit(request);

        Poco::Net::HTTPResponse response;

        std::string retMsg;
        if (DoRequest(session, request, response, formQuery, retMsg, state)) {
            printf("%s\n", retMsg.c_str());
        }
        return retMsg;
    }
    catch (const Poco::Exception &ex) {
        printf("[%s:%d] %s\n", __FILE__, __LINE__, ex.displayText().c_str());
    }

    return "";
}

std::string HttpClient::Post(const std::string &path,
                             const std::string &body,
                             const std::map<std::string, std::string> &header, int &state) {
    try {
        Poco::Net::HTTPClientSession session(m_host, std::stoi(m_port));
        Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_POST, path, Poco::Net::HTTPMessage::HTTP_1_1);
        // 设置请求头
        request.set("User-Agent", "PocoRuntime-PocoRuntime/1.1.0");
        for (auto item: header) {
            request.set(item.first, item.second);
        }

        // XML类型的请求体
        // request.setContentType("application/xml");
        // request.setContentLength(body.length());

        Poco::Net::HTTPResponse response;

        std::string retMsg;
        if (DoRequest(session, request, response, body, retMsg, state)) {
            printf("%s\n", retMsg.c_str());
        }
        return retMsg;
    }
    catch (const Poco::Exception &ex) {
        printf("[%s:%d] %s\n", __FILE__, __LINE__, ex.displayText().c_str());
    }

    return "";
}

bool HttpClient::DoRequest(Poco::Net::HTTPClientSession &session,
                           Poco::Net::HTTPRequest &request,
                           Poco::Net::HTTPResponse &response,
                           std::string &repMsg, int &state) {

    session.sendRequest(request);                         // 发送请求
    std::istream &is = session.receiveResponse(response); // 接收响应
    std::ostringstream oss;
    printf("[%s:%d] %d %s\n", __FILE__, __LINE__, response.getStatus(), response.getReason().c_str());

    state = response.getStatus();
    if (response.getStatus() != Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED) {
        // Poco::StreamCopier::copyStream(rs, std::cout);
        Poco::StreamCopier::copyStream(is, oss);
        if (!oss.str().empty()) {
            // printf("[%s:%d] %s\n", __FILE__, __LINE__, oss.str().c_str());
            repMsg = oss.str();
        }

        return true;
    } else {
        printf("[%s:%d] -----HTTPResponse error-----\n", __FILE__, __LINE__);
        Poco::NullOutputStream null;
        Poco::StreamCopier::copyStream(is, null);
        return false;
    }
}

bool HttpClient::DoRequest(Poco::Net::HTTPClientSession &session,
                           Poco::Net::HTTPRequest &request,
                           Poco::Net::HTTPResponse &response,
                           Poco::Net::HTMLForm &Query,
                           std::string &responseMsg, int &state) {
    std::ostream &os = session.sendRequest(request); // 发送请求
    // 添加请求参数
    Query.write(os);
    os.flush();

    std::istream &is = session.receiveResponse(response); // 接收响应
    std::ostringstream oss;
    printf("[%s:%d] %d %s\n", __FILE__, __LINE__, response.getStatus(), response.getReason().c_str());

    state = response.getStatus();
    if (response.getStatus() != Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED) {
        // Poco::StreamCopier::copyStream(rs, std::cout);
        Poco::StreamCopier::copyStream(is, oss);
        if (!oss.str().empty()) {
            // printf("[%s:%d] %s\n", __FILE__, __LINE__, oss.str().c_str());
            responseMsg = oss.str();
        }

        return true;
    } else {
        printf("[%s:%d] -----HTTPResponse error-----\n", __FILE__, __LINE__);
        Poco::NullOutputStream null;
        Poco::StreamCopier::copyStream(is, null);
        return false;
    }
}

bool HttpClient::DoRequest(Poco::Net::HTTPClientSession &session,
                           Poco::Net::HTTPRequest &request,
                           Poco::Net::HTTPResponse &response,
                           const std::string &Body,
                           std::string &responseMsg, int &state) {
    std::ostream &os = session.sendRequest(request); // 发送请求
    // 添加请求体
    os << Body;
    os.flush();

    std::istream &is = session.receiveResponse(response); // 接收响应
    std::ostringstream oss;
    printf("[%s:%d] %d %s\n", __FILE__, __LINE__, response.getStatus(), response.getReason().c_str());

    state = response.getStatus();
    if (response.getStatus() != Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED) {
        // Poco::StreamCopier::copyStream(rs, std::cout);
        Poco::StreamCopier::copyStream(is, oss);
        if (!oss.str().empty()) {
            // printf("[%s:%d] %s\n", __FILE__, __LINE__, oss.str().c_str());
            responseMsg = oss.str();
        }

        return true;
    } else {
        printf("[%s:%d] -----HTTPResponse error-----\n", __FILE__, __LINE__);

        Poco::NullOutputStream null;
        Poco::StreamCopier::copyStream(is, null);
        return false;
    }
}
