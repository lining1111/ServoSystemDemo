# ServoSystemDemo

    主要依赖Poco库的C++伺服系统代码模板，最后将设
备类接口写出、请求处理过程写到common/proc.cpp中的router中即可完成对应的设备伺服系统

# changelog
 0.0.1
    编写基于tcp的通信架构
 0.0.2
    将websocket加入到架构中，同时加入基于cpp-httplib的httpclient，
    基于cpp-httplib的httpServer可以后续在实际的工程中加入，但是一般cpp的工程不会出现httpServer的模式，
    因为处理的东西太复杂了。一般cpp端都是实现传输层的tcp/udp。而会话层及以上的http、ssh、ftp等，用更高级的语言合适。