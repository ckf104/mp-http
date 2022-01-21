# mp-http

## 目前的各个文件结构：

* `go_dash_server`是一个用go写的http-server，运行后会监听在地址`127.0.0.1:80`，root目录为index.html所在目录。`go_dash_server\server.go`是主要的server相关代码，`helper\helper.go`是一些辅助函数
* `media_src`目录下是参考的dash播放源，`media_src\single`目录下是fmp4文件，通过http中的range header来取视频块，而`media_src\multiple`则是把完整mp4拆分成了若干独立的文件块。
* `dash.js-master`是实现客户端dash播放器的dash.js库
* `index.html` 和 `style.css`是客户端播放器
* `cpp_proxy`下是所有的proxy代码，`general.hpp`存放所有需要的常量，`helper.cpp`是相关的辅助函数，`proxy.cpp`是监听套接字的主函数，`scheduler.cpp`实际处理连接。

## TODO

* 实现c_proxy下的send_mp_request函数

## TEST

* google-chrome -proxy-server=http://localhost:32345

* 在/etc/hosts加入127.0.0.1 local

* 在/etc/hosts中加入

  ```
  10.100.1.2 server
  10.100.2.2 server
  ```

* 执行setup.sh 来构建vnet，运行完毕后执行remove.sh来移除对应的configuration

* 运行代理服务器

* 用chrome访问http://local

* (如果使用master分支的proxy的话)

  * 浏览器建议使用firefox
  * 首先在mp-http主文件夹下执行mkdir build建立build文件夹
  * 进入build文件夹，并执行cmake .. && make
  * 编译完成后在build文件夹执行./proxy $port$来运行proxy, 其中$port$为指定的端口号