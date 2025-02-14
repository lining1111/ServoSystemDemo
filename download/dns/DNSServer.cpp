//
// Created by lining on 2023/2/23.
//

#include "DNSServer.h"

/*dns服务，ip池*/
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>
#include <string.h>
#include <map>
#include "uri.h"
#include <netdb.h>
#include <vector>
#include <glog/logging.h>

using namespace std;
using namespace uri;
namespace myDNS {
/*判断字符串是不是一个有效ip地址,0非ip，1，ip地址*/
    int isIP(char *str) {
        char temp[32];
        int a, b, c, d;

        if (strlen(str) > strlen("255.255.255.255")) {
            return 0;
        }

        if ((sscanf(str, "%d.%d.%d.%d", &a, &b, &c, &d)) != 4) {
            return 0;
        }
        sprintf(temp, "%d.%d.%d.%d", a, b, c, d);
        if (strcmp(temp, str) != 0) {
            return 0;
        }
        if (!((a <= 255 && a >= 0) && (b <= 255 && b >= 0) && (c <= 255 && c >= 0))) {
            return 0;
        }

        return 1;
    }

/*

01  00  ID
81  80  flags
00  01  问题数
00  01  应答数
00  00  授权资源记录数
00  01  附加资源记录数
查询区域：
查询名 fs.aps.aipark.com
02  66  73  fs
03  61  70  73  aps
06  61  69  70  61  72  6B  aipark
03  63  6F  6D  com
00  查询名结束
00  01  查询类型
00  01  查询类
回答区域：
应答资源记录（可以多条，每条都是以C0 0C 00 01 00 01开头）
C0  0C  域名 当报文中域名重复出现的时候，该字段使用2个字节的偏移指针来表示，例子C00C(1100000000001100，12正好是头部的长度，其正好指向Queries区域的查询名字字段)
00  01  查询类型 固定2个字节
00  01  查询类 固定2个字节
00  00  00  10 生存时间TTL  查询类型为1时，为4个字节
00  04  资源数据长度 固定2个字节
2F  5F  32  23  47.95.50.35
授权资源记录                             c  a  i   s   h   i   k   0   u  菜市口
09  63  61  69  73  68  69  6B  6F  75  （99 97 105 115 104 105 107 111 117）
08  72  65  64  69  72  65  63  74  (r e d i r e c t)
00
附加资源记录
00  01  查询类型
00  01  查询类
00  00  0E  10  生存时间TTL
00  04  资源数据长度
7F  00  00  01  127.0.0.1

*/

/*数据包头*/
    typedef struct {
        unsigned char id[2];   /*会话ID---0表示请求,01 00表示接收*/
        unsigned char flag[2]; /*标志:0x81,0x80表示接收成功*/
        unsigned char ques_num[2];   /*询问信息个数*/
        unsigned char ans_num[2];    /*应答信息个数*/
    } DNS_REQ_HEAD;

/*数据报内容*/
    typedef struct {
        unsigned char data_name[2];   /*域名:0xC0,0x0C*/
        unsigned char data_type[2];   /*数据类型1:由域名获得IPv4地址*/
        unsigned char data_class[2];  /*查询类CLASS 1:IN*/
        unsigned char data_time[4];   /*生存时间（TTL）*/
        unsigned char data_len[2];    /*数据长度*/
        unsigned char data_buf[4];    /*ipV4的ip地址信息*/
    } DNS_REQ_DATA;


/*dns内容信息解析*/
    static int dns_data_parse(char *dns_buf, int dns_buf_len, std::vector<std::string> &dns_ip_str_all) {
        int ret = 0;
        char printf_buf[1024];

        memset(printf_buf, 0x0, sizeof(printf_buf));

        for (int i = 0; i < dns_buf_len; i++) {
            if (strlen(printf_buf) < sizeof(printf_buf) - 8) {
                sprintf(printf_buf + strlen(printf_buf), "%02X ", dns_buf[i] & 0xFF);
            }
            char *p_dns_buf = dns_buf + i;
            DNS_REQ_DATA *p_dns_head = (DNS_REQ_DATA *) p_dns_buf;
            if (*(unsigned short *) p_dns_head->data_type == 0x0100 &&
                *(unsigned short *) p_dns_head->data_class == 0x0100) {
                if (*(unsigned short *) p_dns_head->data_len == 0x0400) {
                    char tmp_ip_buf[32];
                    memset(tmp_ip_buf, 0x0, sizeof(tmp_ip_buf));
                    sprintf(tmp_ip_buf, "%u.%u.%u.%u", p_dns_head->data_buf[0],
                            p_dns_head->data_buf[1], p_dns_head->data_buf[2],
                            p_dns_head->data_buf[3]);
                    dns_ip_str_all.push_back(tmp_ip_buf);
                } else {
                    LOG(ERROR) << "数据长度错误" << *(unsigned short *) p_dns_head->data_len;
                }
            }
        }
        LOG(ERROR) << "接收数据:" << printf_buf;
        return 0;
    }

/*dns头部信息解析*/
    static int dns_head_parse(char *dns_buf, int dns_buf_len) {
        int ret = 0;

        DNS_REQ_HEAD *p_dns_head = (DNS_REQ_HEAD *) dns_buf;
        if (*(unsigned short *) p_dns_head->id != 0x0001) {
            LOG(ERROR) << "dns解析错误" << *(unsigned short *) p_dns_head->id;
            return -1;
        }

        if (*(unsigned short *) p_dns_head->flag != 0x8081) {
            LOG(ERROR) << "dns解析错误" << *(unsigned short *) p_dns_head->flag;
            return -1;
        }

        return 0;
    }

/*dns解析*/
    static int dns_parse(char *dns_buf, int dns_buf_len, std::vector<std::string> &dns_ip_str_all) {
        int ret = 0;

        /*头部信息解析*/
        ret = dns_head_parse(dns_buf, dns_buf_len);
        if (ret < 0) {
            LOG(ERROR) << "dns解析失败";
            return -1;
        }

        /*dns内容信息解析*/
        ret = dns_data_parse(dns_buf, dns_buf_len, dns_ip_str_all);
        if (ret < 0) {
            LOG(ERROR) << "dns内容信息解析失败";
            return -1;
        }

        return 0;
    }

/*
参数：
1）host_name，需要解析的域名
2）host_ip，解析出的IP
返回值：0成功，-1失败
*/
    static int dns_resolve_114(const char *dnsip, const char *host_name, char *host_ip) {
#define BUF_SIZE 1024
#define SRV_PORT 53

        if (NULL == host_name || NULL == host_ip) {
            LOG(ERROR) << "dns_resolve_114 host_name=" << host_name << " host_ip=" << host_ip;
            return -1;
        }

        typedef unsigned short U16;
        //const char srv_ip[] = "114.114.114.114";
        typedef struct _DNS_HDR {
            U16 id;
            U16 tag;
            U16 numq;
            U16 numa;
            U16 numa1;
            U16 numa2;
        } DNS_HDR;
        typedef struct _DNS_QER {
            U16 type;
            U16 classes;
        } DNS_QER;

        int clifd, len = 0, i;
        struct sockaddr_in servaddr;
        char buf[BUF_SIZE];
        char *ptr;
        char *tmp_ptr;
        DNS_HDR *dnshdr = (DNS_HDR *) buf;
        DNS_QER *dnsqer = (DNS_QER *) (buf + sizeof(DNS_HDR));

        timeval tv = {5, 0};

        if ((clifd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
            LOG(ERROR) << "dns_resolve_114 create socket error!";
            return -1;
        }
        setsockopt(clifd, SOL_SOCKET, SO_RCVTIMEO, (char *) &tv, sizeof(timeval));

        bzero(&servaddr, sizeof(servaddr));
        servaddr.sin_family = AF_INET;
        inet_aton(dnsip, &servaddr.sin_addr);
        servaddr.sin_port = htons(SRV_PORT);
        memset(buf, 0, BUF_SIZE);
        dnshdr->id = (U16) 1;
        dnshdr->tag = htons(0x0100);
        dnshdr->numq = htons(1);
        dnshdr->numa = 0;

        strcpy(buf + sizeof(DNS_HDR) + 1, host_name);
        ptr = buf + sizeof(DNS_HDR) + 1;
        i = 0;
        while (ptr < (buf + sizeof(DNS_HDR) + 1 + strlen(host_name))) {
            if (*ptr == '.') {
                *(ptr - i - 1) = i;
                i = 0;
            } else {
                i++;
            }
            ptr++;
        }
        *(ptr - i - 1) = i;

        dnsqer = (DNS_QER *) (buf + sizeof(DNS_HDR) + 2 + strlen(host_name));
        dnsqer->classes = htons(1);
        dnsqer->type = htons(1);
        len = sendto(clifd, buf, sizeof(DNS_HDR) + sizeof(DNS_QER) + strlen(host_name) + 2, 0,
                     (struct sockaddr *) &servaddr, sizeof(servaddr));

        i = sizeof(struct sockaddr_in);
        len = recvfrom(clifd, buf, BUF_SIZE, 0, (struct sockaddr *) &servaddr, (socklen_t *) &i);
        close(clifd);
        if (len < 0) {
            LOG(ERROR) << "dns_resolve_114 recv error";
            return -1;
        }
        if (dnshdr->numa == 0) {
            LOG(ERROR) << "dns_resolve_114 ack error";
            return -1;
        }
        ptr = buf + len - 4;
        tmp_ptr = ptr;

        std::vector<std::string> dns_ip_str_all;
        dns_parse(buf, len, dns_ip_str_all);

        if (dns_ip_str_all.size() == 0) {
            LOG(ERROR) << "dns_resolve_114 dns_parse error";
            return -1;
        }

        for (int i = 0; i < (int) dns_ip_str_all.size(); i++) {
            LOG(INFO) << "host" << host_name << "解析dns:" << dns_ip_str_all[i].c_str();
        }

        //sprintf(host_ip,"%u.%u.%u.%u",(unsigned char)*ptr, (unsigned char)*(ptr + 1), (unsigned char)*(ptr + 2), (unsigned char)*(ptr + 3));
        sprintf(host_ip, "%s", (char *) dns_ip_str_all[0].c_str());
        return 0;
    }


/*解析url*/
    static int uri_parse(char *sorcpath, char *urihost, char *uripath) {
        struct ast_uri *uri_s = ast_uri_parse(sorcpath);

        if (uri_s->host != NULL) {
            sprintf(urihost, "%s", uri_s->host);
        }

        if (uri_s->path != NULL) {
            sprintf(uripath, "/%s", uri_s->path);
        }

        free(uri_s);

        return 0;
    }

/*解析dns:

hoststr:域名;
ipaddr:ip地址;
ret = 0 成功
*/
    int url_get(std::string hoststr, std::string &ipaddr) {
        struct hostent hostinfo, *phost;
        char tmpipaddr[64];
        char *buf;
        int ret;
        char dnsserver[8][32] = {
                "119.29.29.29",        //public DNS+
                "182.254.116.116",        //public DNS+
                "114.114.114.114",
                "223.5.5.5",            //ali
                "223.6.6.6",            //ali
                "180.76.76.76",        //baidu
                "8.8.8.8",                //google
                "8.8.4.4"                //google
        };
        buf = (char *) malloc(1024);
        if (buf == NULL) {
            LOG(ERROR) << "malloc 错误";
            return -1;
        }
        memset(buf, 0x0, 1024);
        memset(tmpipaddr, 0x0, sizeof(tmpipaddr));

        /*取得主机IP地址*/
        if (gethostbyname_r((char *) hoststr.c_str(), &hostinfo, buf,
                            1024, &phost, &ret)) {
            free(buf);
            LOG(ERROR) << "gethostbyname_r " << hoststr << " ret:" << ret;
            int i;
            int size = sizeof(dnsserver) / 64;

            for (i = 0; i < size; i++) {
                ret = dns_resolve_114(dnsserver[i], (char *) hoststr.c_str(),
                                      tmpipaddr);
                if (ret < 0) {
                    LOG(ERROR) << "dns_resolve_114 通过" << dnsserver[i] << "获取ip地址失败";
                    continue;
                }
                ipaddr = tmpipaddr;
                return 0;
            }
            LOG(ERROR) << "http  dns未查找到" << hoststr << "域名的ip";
            return -1;
        }
        if (phost == NULL) {
            LOG(ERROR) << "dns_resolve_114 通过114.114.114.114获取ip地址失败";
            free(buf);
            return -1;
        }

        inet_ntop(AF_INET, (struct in_addr *) hostinfo.h_addr_list[0],
                  tmpipaddr, sizeof(tmpipaddr));
        ipaddr = tmpipaddr;
        //DBG("gethostbyname_r获取host:%s,ip地址:%s",
        //         (char *)hoststr.c_str(),tmpipaddr);
        free(buf);
        return 0;
    }

/*查询dns解析表:
url:解析url地址;
url_ip:将域名解析成ip的地址;
force:强制解析域名 0,不强制获取，1 强制获取 2,更新dns
*/
    int searchDNS(std::string url, std::string &url_ip,
                  int force, std::string &host, std::string &port, std::string &ip_addr) {
        static pthread_mutex_t dns_table_lock = PTHREAD_MUTEX_INITIALIZER; /*泊位临时数据库线程锁*/
#define DNS_NUMBER   32
        static std::map<std::string, std::string> dns_table;
        std::string hoststr;
        std::string ipaddr;
        int i;
        int ret;

        if (force == 2) {
            //DBG("更新dns");
            pthread_mutex_lock(&dns_table_lock);
            std::map<std::string, std::string>::iterator iter;//定义一个迭代指针iter
            for (iter = dns_table.begin(); iter != dns_table.end(); iter++) {
                //DBG("更新dns[%s:%s]",iter->first,iter->second);
                ret = url_get(iter->first, ipaddr);
                if (ret < 0) {
                    LOG(ERROR) << "获取ip地址失败";
                }
                if (strlen(ipaddr.c_str()) > 0) {
                    iter->second = ipaddr;
                }
            }
            pthread_mutex_unlock(&dns_table_lock);
            return 0;
        }

        if (url.length() == 0) {
            LOG(ERROR) << "查询dns错误,url值为空";
            return -1;
        }
        url_ip = url;
        //DBG("查询URL:%s",(char *)url.c_str());
        struct ast_uri *uri_s = ast_uri_parse((char *) url.c_str());
        if (uri_s->host == NULL) {
            LOG(ERROR) << "查询dns错误,url:" << url;
            return -1;
        }
        hoststr = uri_s->host;
        host = uri_s->host;
        if (uri_s->port != NULL) {
            port = uri_s->port;
        } else {
            port = "80";
        }
        free(uri_s);

        //DBG("host:%s",(char *)hoststr.c_str());
        if (isIP((char *) host.c_str()) == 1) {
            LOG(ERROR) << "url:" << url << "的host地址是IP不进行解析";
            return -1;
        }
        pthread_mutex_lock(&dns_table_lock);
        std::map<std::string, std::string>::iterator tmpit;
        switch (force) {
            case 0:/*不强制获取*/
                //DBG("查询dns");
                tmpit = dns_table.find(hoststr);
                if (tmpit == dns_table.end()) {
                    ret = url_get(hoststr, ipaddr);
                    if (ret < 0) {
                        LOG(ERROR) << "获取ip地址失败";
                        pthread_mutex_unlock(&dns_table_lock);
                        return -1;
                    }
                    if (strlen(ipaddr.c_str()) > 0) {
                        dns_table[hoststr] = ipaddr;
                    }
                } else {
                    ipaddr = dns_table[hoststr];
                }
                break;
            case 1:/*强制获取*/
                //DBG("强制获取dns");
                ret = url_get(hoststr, ipaddr);
                if (ret < 0) {
                    LOG(ERROR) << "获取ip地址失败";
                    pthread_mutex_unlock(&dns_table_lock);
                    return -1;
                }
                if (strlen(ipaddr.c_str()) > 0) {
                    dns_table[hoststr] = ipaddr;
                }
                break;
            default:
                tmpit = dns_table.find(hoststr);
                if (tmpit == dns_table.end()) {
                    ret = url_get(hoststr, ipaddr);
                    if (ret < 0) {
                        LOG(ERROR) << "获取ip地址失败";
                        pthread_mutex_unlock(&dns_table_lock);
                        return -1;
                    }
                    if (strlen(ipaddr.c_str()) > 0) {
                        dns_table[hoststr] = ipaddr;
                    }
                } else {
                    ipaddr = dns_table[hoststr];
                }
                break;
        }
        pthread_mutex_unlock(&dns_table_lock);
        url_ip = url_ip.replace(url_ip.find(hoststr), hoststr.length(), ipaddr);
        if (url_ip.find("http://") != std::string::npos) {
            url_ip = url_ip.replace(url_ip.find("http://"), strlen("http://"), "");
        }
        //DBG("返回url_ip=%s",(char *)url_ip.c_str());
        return 0;
    }

/*dns服务*/
    static void *dns_server_task(void *argv) {
        while (1) {
            std::string tmpurl, tmpip, tmphost, tmpport, tmp_ipaddr;
            searchDNS(tmpurl, tmpip, 2, tmphost, tmpport, tmp_ipaddr);
            sleep(60 * 30);/*30分钟进行一次dns的更新*/
        }
        return NULL;
    }

/*dns服务*/
    int DNSServerStart() {
        pthread_t tid;
        int ret;

        ret = pthread_create(&tid, NULL, dns_server_task, NULL);
        if (ret != 0) {
            LOG(ERROR) << "Can't create dns_server_start()";
            return -1;
        } else {
            LOG(INFO) << "Create dns_server_start() succeed";
            return 0;
        }
    }
}