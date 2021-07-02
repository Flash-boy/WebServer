# TinyWebServer
主要是参考Linux高性能服务器编程，游双著.以及[@qinguoyi](https://github.com/qinguoyi)编写的一个简易的TinyWebServer

- 使用了线程池+非阻塞socket+epoll(ET,LT均实现)+事件处理(Reactor和Practor模式)
- 利用状态机解析http请求报文，支持Get和Post请求
- 利用了一个简易数据库连接池和RAII管理数据库对象，一个简易登录注册功能
- 利用阻塞队列实现异步日志系统
  
