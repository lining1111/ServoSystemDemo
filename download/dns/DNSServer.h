//
// Created by lining on 2023/2/23.
//

#ifndef DNSSERVER_H
#define DNSSERVER_H

#include <string>

namespace myDNS {
    int searchDNS(std::string url, std::string &url_ip,
                  int force, std::string &host, std::string &port, std::string &ip_addr);
    int isIP(char *str);
    int url_get(std::string hoststr, std::string &ipaddr);

/*dns服务*/
    int DNSServerStart();

}
#endif //DNSSERVER_H
