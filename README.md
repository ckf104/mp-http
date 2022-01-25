# mp-http

## 目前的各个文件结构：

* `go_dash_server`是一个用go写的http-server，通过`go run server.go <ipaddr>`运行，server的root目录为index.html所在目录。`go_dash_server\server.go`是主要的server相关代码，`helper\helper.go`是一些辅助函数
* `media_src`目录下是参考的dash播放源，`media_src\single`目录下是fmp4文件，通过http中的range header来取视频块，而`media_src\multiple`则是把完整mp4拆分成了若干独立的文件块，在请求时不会有range header。`media_src\high_multi`是仅有最高质量视频的dash视频流，用于report 6.2节的评测。因为视频文件过大，可以通过北大网盘获取，在相应的目录下解压即可。

  * `multiple`目录下: https://disk.pku.edu.cn:443/link/88484FB25D41BBAA7E267154DB860748
    有效期限：2022-02-22 23:59
  * `single`目录下: https://disk.pku.edu.cn:443/link/FB7B3671014AF39726B18A18E7185CBD
    有效期限：2022-02-22 23:59
  * `high_multi`目录下: https://disk.pku.edu.cn:443/link/8F16BE08A17090385776B886CF87C40D
    有效期限：2022-02-23 23:59
* `dash.js-master`是实现客户端dash播放器的dash.js库
* `index.html` 和 `style.css`是一个简单的客户端播放器
* `src`是代理服务器的代码

## TEST

* 在ubuntu系统下使用google-chrome浏览器，设置其使用系统代理.

* 设置系统代理, http proxy : addr 127.0.0.1, port 32345. 并且在`ignore host`选项中删除`127*`, 使得chrome对于本地地址也使用代理。

* 在/etc/hosts中加入

  ```
  10.100.1.2 server
  10.100.2.2 server
  ```

* 执行setup.sh 来构建vnet，测试完毕后执行remove.sh来移除对应的configuration

* 运行代理服务器

  * 首先在mp-http主文件夹下执行mkdir build建立build文件夹
  * 进入build文件夹，并执行cmake .. && make
  * 编译完成后在build文件夹执行`./proxy 32345 1`来运行proxy, 其中第一个参数为指定的端口号，第二个参数表明是否使用多路代理，如果为0表示disable.

* 用chrome访问http://server, 在html页面的输入栏中输入一个mpd文件的url地址（例如 http://server/single/360p_dash.mpd），点击load加载即可。