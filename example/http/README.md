# WebServer ![](https://img.shields.io/badge/license-MIT-blue) ![](https://img.shields.io/badge/C%2B%2B-17-green) ![](https://img.shields.io/badge/os-ubuntu%2020.04-brightgreen)
Reactor，线程池，定时器与HTTP协议解析（支持GET与POST）

## 1 文件结构
├── cgi-bin  存放CGI程序  
│   ├── exp  示例CGI程序可执行文件    
│   └── exp.c  示例CGI程序源代码  
├── favicon.ico  网页图标  
├── hello.html  示例静态网页  
├── log  简易日志系统输出目录  
└── src  源代码  
    ├── http_parser.cpp  HTTP协议解析     
    ├── http_parser.hpp  
    ├── log.cpp  简易日志系统  
    ├── log.hpp  
    ├── main.cpp  示例main程序  
    ├── thread_pool.hpp  线程池  
    ├── timer.cpp  定时器与定时器容器  
    ├── timer.hpp  
    └── webserver.hpp  包含Acceptor， Web事件处理器， Reactor  

## 2 使用方法
只需包含webserver.hpp，然后启动反应堆，参考main.cpp内写法即可。

## 3 总体架构
![](/pic/struct.png)

## 4 事件处理模式概述——基于IO复用的Reactor
![](/pic/epoll.png)

## 5 定时器机制概述
>### 5.1 超时与否的确认
定时器采用绝对时间；若系统当前时间超过某定时器内部设定的绝对时间，则认为该定时器超时。
>### 5.2 定时器容器
所有定时器均存储在一个定时器容器中；此处采用的是红黑树（没有自己写，用的std::set）。
>### 5.3 心跳机制
采用alarm设定一个周期（CYCLE_TIME），该周期即为心跳周期（心跳函数tick的执行周期）；  
心跳函数的任务是查看定时器容器中有没有已超时的定时器；若有就进行其对应的超时处理并删除该定时器。

## 6 线程池概述
此处线程池由工作队列与存储线程的数组实现。  
>### 6.1 工作队列
工作队列即缓存区，由std::queue实现。其内部存储线程待执行的某一任务，而“任务”由多态与继承实现。
>### 6.2 内部存储的线程
存储线程的数量由模板参数指定。  
每个线程内均绑定了一个_run函数，其内部从工作队列取出任务来执行；  
工作队列与线程间的状态通信由条件变量实现(此处条件变量及互斥锁均采用C++17标准中的内容，而非Linux提供的系统调用)。

## 7 响应HTTP请求概述
>### 7.1 响应静态请求(GET一个静态文件)
打开目标文件并写入字符串，与响应头组成响应报文发送即可。
>### 7.2 响应动态请求(GET一个CGI程序执行结果，POST向CGI程序提交参数)
>#### 7.2.1 获取参数 
GET请求在url链接里自带CGI程序参数；参数与CGI程序名用?分割，参数之间用&分割。
POST请求的body里为CGI程序参数。
>#### 7.2.2 输入输出
在fork出一个子进程来执行CGI程序之前，先建立两个匿名管道，一个重定向至stdin，一个重定向至stdout。   
GET请求的CGI程序参数输入通过环境变量QUERY_STRING传递，结果输出至标准输出；由于已经重定向到匿名管道，因此父进程读取匿名管道即可。   
POST请求的CGI程序参数输入通过设置环境变量CONTENT_LENGTH来确定参数长度，再将参数写至已重定向到stdin的匿名管道的写端；结果输出同上。
>### 7.3 获取HTTP请求报文body的长度
>#### 7.3.1 请求头部包含Content-Length字段
该字段的值即为body长度。
>#### 7.3.2 请求头部包含Transfer-Encoding字段
若该字段值为Chunked，则分块读取body，具体可详见[分块传输编码](https://zh.wikipedia.org/wiki/%E5%88%86%E5%9D%97%E4%BC%A0%E8%BE%93%E7%BC%96%E7%A0%81)。   
注： 其余情况，默认不读取body。

## 8 测试结果
测试时采用谷歌浏览器postman插件发送请求。
>### 8.1 响应POST请求（执行CGI示例程序）
![](/pic/post_cgi.png)
>### 8.2 响应GET请求（执行CGI示例程序）
![](/pic/get_cgi.png)
>### 8.3 响应GET请求（返回静态html网页）
![](/pic/get_static.png)
