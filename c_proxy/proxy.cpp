#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <poll.h>
#include <unistd.h>
#include <cstdint>
#include <future>
#include <iostream>
#include <regex>
#include <map>
#include "helper.hpp"
#include "general.hpp"
#include <string>
#include <memory>
#include <set>
#include <assert.h>
#include <optional>
#include <sstream>
#include <chrono>
#include <future>
#define MAX_OBJECT_SIZE 1024000
using std::map;
using std::set;
using std::string;

/*static const char *user_agent_hdr = "Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *conn_hdr = "close\r\n";
static const char *proxy_hdr = "close\r\n";*/
static string range_header("Range");
static string content_range_header("Content-Range");
static string host_header("Host");
static string content_len_hdr("Content-Length");
static string connect_response("HTTP/1.1 200 Connection established\r\n\r\n");
static string connect_method("CONNECT");
int mp = 1;

extern char **environ;
struct HttpStream;
using StreamPtr = std::unique_ptr<HttpStream>;

void doit(int fd);
void clienterror(int fd, const char *cause, const char *errnum, const char *shortmsg, const char *longmsg);
void parse_uri(char *uri, char *hostname, char *path, int *port);
void *Thread(void *vargp);
void do_relay(HttpStream *client);

mp_http need_mp_http(const Request &req, std::pair<uint64_t, uint64_t> &range);
std::optional<Response> send_normal_request(StreamPtr &client, StreamPtr (&servers)[path_num], const Request &client_req, Body &req_body, Body &reply_body);
std::optional<Response> send_mp_request(StreamPtr &client, StreamPtr (&servers)[path_num], const Request &client_req,
                                        Body &req_body, Body &reply_body, mp_http type, const std::pair<uint64_t, uint64_t> &byte_range,
                                        std::future<std::pair<int, int>> &fu);

struct HttpStream
{
public:
    Rio_t fd;
    uint other_ip_addr = 0;
    uint latency = 0;   // unit ms
    uint bandwidth = 0; // unit kB
    string hostname;
    int port;
    HttpStream(int _fd) : fd(_fd) {}
    void close()
    {
        if (fd)
            Close(fd.rio_fd);
        fd.rio_fd = 0;
    }
    /**
     * @brief factory method for HttpStream
     * 
     * @param _fd file descriptor of the stream
     * @return if this function returns null, the stream is closed.
     */
    static StreamPtr generateStream(int _fd)
    {
        auto r = std::make_unique<HttpStream>(_fd);
        return r;
    }
    operator bool()
    {
        return fd;
    }
    /**
     * @brief Get the Request object
     * 
     * @return int 0 means success 
     */
    std::optional<Request> getRequest(Body &req_body)
    {
        //char _method[max_line], _uri[max_line], _version[max_line];
        //char _hostname[max_line], _path[max_line];
        //req_body.clear();
        Request r;
        if (!fd.rio_readlineb(r.headline))
        {
            return std::nullopt;
        }
        std::istringstream first_line(r.headline);
        //sscanf(buf.c_str(), "%s %s %s", _method, _uri, _version);

        first_line >> r.method >> r.url >> r.http_proto; /* r.http_proto = http/1.1/r/n */

        /*r.method = string(_method);
        r.url = string(_uri);
        r.http_proto = string(_version);
        parse_uri(_uri, _hostname, _path, &port);
        r.hostname = hostname = string(_hostname);
        r.path = string(_path);*/
        /*if (r.method != "GET")
        {
            //printf("[Error] Not implemented!.\n");
            return std::nullopt;
        }*/
        std::cout << r.url << " " << r.method << std::endl;
        build_requesthdrs(r.headers);
        if (r.headers.count(content_len_hdr) == 1)
        {
            auto leng = std::stoul(r.headers.at(content_len_hdr));
            if (fd.rio_readnb(req_body, leng) != (ssize_t)leng)
                return std::nullopt;
        }
        const string &host = r.headers.at(host_header);
        auto index = host.find(':'); // Host: <hostname>:<port>
        if (index == string::npos)
        {
            hostname = host.substr(0, host.size() - 2);
            port = 80;
        }
        else
        {
            hostname = host.substr(0, index);
            port = std::stoi(host.substr(index + 1));
            /*if (port != 80)
            {
                printf("[Error] Not implemented!.\n");
                return std::nullopt;
            }*/
        }
        return std::make_optional(r);
    }

    std::optional<Response> getResponse()
    {
        Response r;
        int n;
        string buf;
        n = fd.rio_readlineb(r.headline);
        char ver[max_line], code[max_line], status[max_line];
        sscanf(r.headline.c_str(), "%s%s%s", ver, code, status);
        r.version = string(ver);
        r.code = string(code);
        r.status = string(status);

        while ((n = fd.rio_readlineb(buf)) > 0)
        {
            if (buf == "\r\n")
                break;
            int p;
            ssize_t len = buf.size();
            for (p = 0; p < len; ++p)
                if (buf[p] == ':')
                    break;
            if (p < len)
                //TODO:Not robust
                r.headers[buf.substr(0, p)] = buf.substr(p + 2, len);
        }
        return std::make_optional(r);
    }
    void build_requesthdrs(map<string, string> &headers)
    {
        headers.clear();
        string buf;
        while (fd.rio_readlineb(buf) > 0)
        {
            if (buf == "\r\n")
                break;
            int p;
            ssize_t len = buf.size();
            for (p = 0; p < len; ++p)
                if (buf[p] == ':')
                    break;
            if (p < len)
            {
                headers[buf.substr(0, p)] = buf.substr(p + 2, len);
            }
        }
    }
    static bool sendReqeust(Rio_t &w, const Request &r, const Body &body)
    {
        char buf[max_line];
        sprintf(buf, "GET %s HTTP/1.1\r\n", r.url.c_str());
        if (w.rio_writen((void *)buf, strlen(buf), MSG_MORE) < 0)
            return false;

        for (auto x : r.headers)
        {
            /*
            if (x.first == "Host")
                sprintf(buf, " %s: %s", x.first.c_str(), x.second.c_str());
            else
                sprintf(buf, "%s: %s", x.first.c_str(), x.second.c_str());
            */
            sprintf(buf, "%s: %s", x.first.c_str(), x.second.c_str());
            if (w.rio_writen((void *)buf, strlen(buf), MSG_MORE) < 0)
                return false;
        }
        sprintf(buf, "\r\n");

        if (w.rio_writen((void *)buf, strlen(buf)) < 0 || w.rio_writen(body.content.get(), body.length) < 0)
            return false;
        return true;
    }
    static bool sendResponse(Rio_t &w, const Response &r, const Body &reply_body)
    {
        char buf[max_line];
        if (w.rio_writen((void *)r.headline.c_str(), r.headline.size()) < 0)
            return false;
        for (auto x : r.headers)
        {
            sprintf(buf, "%s: %s", x.first.c_str(), x.second.c_str());
            if (w.rio_writen((void *)buf, strlen(buf)) < 0)
                return false;
        }
        sprintf(buf, "\r\n");
        if (w.rio_writen((void *)buf, strlen(buf)) < 0)
            return false;
        if (w.rio_writen(reply_body.content.get(), reply_body.length) < 0)
            return false;
        return true;
    }
};

// returning true means pass
bool check_repeat(StreamPtr (&servers)[path_num], uint addr)
{
    for (int i = 0; i < path_num; ++i)
    {
        if (servers[i]->other_ip_addr == addr)
            return false;
    }
    return true;
}

// choose a null server
HttpStream *choose_server(StreamPtr (&servers)[path_num])
{
    for (int i = 0; i < path_num; ++i)
    {
        if (!*servers[i].get())
        {
            return servers[i].get();
        }
    }
    return nullptr;
}

// connect servers[0] if not mp-http, else servers[0] or servers[1] or servers[0] and servers[1]
int Open_mp_clientfd(const char *hostname, const char *port, StreamPtr (&servers)[path_num])
{
    int clientfd, rc;
    struct addrinfo hints, *listp, *p;

    /* Get a list of potential server addresses */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_socktype = SOCK_STREAM; /* Open a connection */
    hints.ai_flags = AI_NUMERICSERV; /* ... using a numeric port arg. */
    hints.ai_flags |= AI_ADDRCONFIG; /* Recommended for connections */
    if ((rc = getaddrinfo(hostname, port, &hints, &listp)) != 0)
    {
        fprintf(stderr, "getaddrinfo failed (%s:%s): %s\n", hostname, port, gai_strerror(rc));
        return -2;
    }

    /* Walk the list for one that we can successfully connect to */
    for (p = listp; p; p = p->ai_next)
    {
        /* Create a socket descriptor */
        if (!check_repeat(servers, ((sockaddr_in *)p->ai_addr)->sin_addr.s_addr))
            continue;
        auto server = choose_server(servers);
        if (!server)
            break;
        if ((clientfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0)
            continue; /* Socket failed, try the next */

        /* Connect to the server */
        auto now = std::chrono::system_clock::now();
        if (connect(clientfd, p->ai_addr, p->ai_addrlen) != -1)
        {
            server->fd.rio_fd = clientfd; /* Success */
            server->other_ip_addr = ((sockaddr_in *)p->ai_addr)->sin_addr.s_addr;
            server->latency = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - now).count();
            continue;
        }
        //perror("connect failed: ");
        if (close(clientfd) < 0)
        { /* Connect failed, try another */ //line:netp:openclientfd:closefd
            fprintf(stderr, "open_clientfd: close failed: %s\n", strerror(errno));
            return -1;
        }
    }

    /* Clean up */
    freeaddrinfo(listp);
    if (!p) /* All connects failed */
        return -1;
    else /* The last connect succeeded */
        return clientfd;
}

std::vector<uint64_t> dispatch_work(StreamPtr (&servers)[path_num], uint64_t work_byte_size)
{
    if (path_num != 2)
    {
        printf("dispatch_work: path_num != 2");
        exit(-1);
    }
    else
    {
        if (servers[0]->bandwidth == 0 || servers[1]->bandwidth == 0)
        {
            auto ret_0 = work_byte_size * servers[1]->latency / (servers[0]->latency + servers[1]->latency);
            return {ret_0, work_byte_size - ret_0};
        }
        else
        {
            uint64_t ret_0;
            if (servers[1]->latency > servers[0]->latency)
                ret_0 = (work_byte_size * servers[0]->bandwidth +
                         servers[0]->bandwidth * servers[1]->bandwidth * (servers[1]->latency - servers[0]->latency)) /
                        (servers[0]->bandwidth + servers[1]->bandwidth);
            else
                ret_0 = (work_byte_size * servers[0]->bandwidth -
                         servers[0]->bandwidth * servers[1]->bandwidth * (servers[0]->latency - servers[1]->latency)) /
                        (servers[0]->bandwidth + servers[1]->bandwidth);
            return {ret_0, work_byte_size - ret_0};
        }
    }
}

void update_latency(uint64_t new_latency, HttpStream *server)
{
    if (server->latency != 0)
        server->latency = server->latency / 2 + new_latency / 2;
    else
        server->latency = new_latency;
}

void update_bandwidth(uint64_t time_spent, uint64_t work_size, HttpStream *server)
{
    if (time_spent == 0)
        return;
    if (server->bandwidth != 0)
        server->bandwidth = server->bandwidth / 2 + work_size / (time_spent * 2);
    else
        server->bandwidth = work_size / time_spent;
}

bool check_mp_over(const std::vector<uint64_t> &work)
{
    for (size_t j = work.size(), i = 0; i < j; ++i)
    {
        if (work[i] != 0)
            return false;
    }
    return true;
}

bool check_mp_response(const Response &r)
{
    if (r.code.compare("206") != 0)
        return false;
    else if (r.headers.count(content_range_header) == 0)
        return false;
    else
        return true;
}

// la_i + work_i / b_i = (s - work_i) / b_{1-i}
// time_2 = (la_i * b_i + rest_work) / (b_1 + b_2)
std::vector<uint64_t> decide_send_new_req(StreamPtr (&servers)[path_num], uint64_t rest_work, int i /*server_number*/)
{
    // TODO
    if (path_num != 2)
    {
        printf("decide_send_new_req: pathnum != 2");
        exit(-1);
    }
    vector<uint64_t> ret{0, 0};
    ret[i] = rest_work >= servers[i]->latency * servers[1 - i]->bandwidth ? (rest_work - servers[i]->latency * servers[1 - i]->bandwidth) * servers[i]->bandwidth / (servers[i]->bandwidth + servers[1 - i]->bandwidth) : 0;
    if (ret[i] < new_req_threshold)
        ret[i] = 0;
    if (ret[i] >= rest_work)
    {
        printf("decide_send_new_req: error work distribution");
        exit(-1);
    }
    ret[1 - i] = rest_work - ret[i];
    return ret;
}

int find_server_number(int fd, StreamPtr (&servers)[path_num])
{
    for (int i = 0; i < path_num; ++i)
    {
        if (fd == servers[i]->fd.rio_fd)
            return i;
    }
    printf("find_server_number: illegal fd\n");
    exit(-1);
}

class workState
{
public:
    std::pair<uint64_t, uint64_t> byte_range;
    std::vector<uint64_t> works;
    std::vector<uint64_t> now_rest;
    uint64_t start_point[path_num]; // within byte_range
    uint8_t *now_buffer_ptr[path_num];
    bool has_read_header[path_num];
    bool need_cancel[path_num];
    std::unique_ptr<pollfd[]> fd_pool;
    int fd_pool_len;
    decltype(std::chrono::system_clock::now()) send_req_time[path_num];
    decltype(std::chrono::system_clock::now()) get_header_time[path_num];
    std::future<std::pair<int, int>> &reconnect_rel;

    void check_future(StreamPtr (&servers)[path_num], const Request &req)
    {
        if (reconnect_rel.valid() && reconnect_rel.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            auto ret = reconnect_rel.get();
            if (servers[ret.first]->fd.rio_fd != 0)
            {
                printf("check_future: error servers[i]->fd\n");
                exit(-1);
            }
            if (ret.second != 0)
            {
                printf("reconnecting server %i succeed\n", ret.first);
                servers[ret.first]->fd.rio_fd = ret.second;
                if (fd_pool_len + 1 != path_num)
                {
                    printf("check_future: why fd_pool_len + 1 != path_num ?\n");
                    exit(-1);
                }
                init_fd_pool(servers, fd_pool_len + 1);
                need_cancel[ret.first] = 0;
                send_new_req(servers, ret.first, req);
            }
        }
    }

    void init_fd_pool(StreamPtr (&servers)[path_num], int expected = -1)
    {
        int su = 0;
        for (int i = 0; i < path_num; ++i)
        {
            if (*servers[i])
                su++;
        }
        fd_pool.reset(new pollfd[su]);
        fd_pool_len = su;
        su = 0;
        for (int i = 0; i < path_num; ++i)
        {
            if (*servers[i])
            {
                fd_pool[su].fd = servers[i]->fd.rio_fd;
                fd_pool[su].events = POLLIN;
                fd_pool[su].revents = 0;
                su++;
            }
        }
        if (expected != -1 && fd_pool_len != expected)
        {
            printf("init_fd_pool: expected != fd_pool_len\n");
            exit(-1);
        }
    }

    workState(const std::pair<uint64_t, uint64_t> byteRange, StreamPtr (&servers)[path_num], const Body &reply_body,
              std::future<std::pair<int, int>> &fu) : reconnect_rel(fu)
    {
        if (reconnect_rel.valid())
        {
            printf("workstate: reconnect_rel.valid() = true!\n");
            exit(-1);
        }
        byte_range = byteRange;
        works = dispatch_work(servers, byte_range.second - byte_range.first + 1);
        printf("two path: %ld bytes and %ld bytes\n", works[0], works[1]);
        if ((int64_t)works[0] < 0 || (int64_t)works[1] < 0) // test code
        {
            printf("\n\n!!!!!!!!!!!!err!!!!!!!!!!!!!!!!!!!!!\n\n");
            exit(-1);
        }
        now_rest = works;
        for (int i = 0; i < path_num; ++i)
        {
            if (i == 0)
            {
                now_buffer_ptr[i] = reply_body.content.get();
                start_point[i] = byte_range.first;
            }
            else
            {
                now_buffer_ptr[i] = now_buffer_ptr[i - 1] + works[i - 1];
                start_point[i] = start_point[i - 1] + works[i - 1];
            }
        }
        memset(has_read_header, 0, sizeof(has_read_header));
        memset(need_cancel, 0, sizeof(need_cancel));

        init_fd_pool(servers, path_num);
    }
    bool send_req(const Request &req, StreamPtr (&servers)[path_num])
    {
        char tmp_buf[100];
        auto r = req;
        for (int i = 0; i < path_num; ++i)
        {
            sprintf(tmp_buf, "bytes=%ld-%ld\r\n", start_point[i], start_point[i] + works[i] - 1);
            r.headers.at(range_header) = tmp_buf;
            if (!servers[i]->sendReqeust(servers[i]->fd, r, Body{}))
                return false;
        }
        auto now = std::chrono::system_clock::now();
        for (int i = 0; i < path_num; ++i)
            send_req_time[i] = now;
        return true;
    }

    bool decide_continue(StreamPtr (&servers)[path_num], int i /*server_number*/, const Request &req)
    {
        if (path_num != 2)
        {
            printf("[err] send_new_req does not be implemented for case path_num != 2");
            exit(-1);
        }
        using namespace std::chrono;
        if (now_rest[i] == 0)
        {
            auto now_time = system_clock::now();
            //for (int i = 0; i < path_num; ++i)
            {
                //    if (works[i] != 0)
                update_bandwidth(duration_cast<milliseconds>(now_time - get_header_time[i]).count(),
                                 works[i] - now_rest[i], servers[i].get());
                if (servers[1 - i]->bandwidth == 0)
                    update_bandwidth(duration_cast<milliseconds>(now_time - get_header_time[1 - i]).count(),
                                     works[1 - i] - now_rest[1 - i], servers[1 - i].get());
                printf("server %d over, bandwidth 0 %d, bandwidth 1 %d, latency 0 %d, latency 1 %d\n",
                       i, servers[0]->bandwidth, servers[1]->bandwidth, servers[0]->latency, servers[1]->latency);
            }
            if (need_cancel[i])
            {
                printf("cancelling server %d\n", i);
                servers[i]->close();
                reconnect_rel = std::async(std::launch::async, reconnect, servers[i]->other_ip_addr, (uint16_t)servers[i]->port, i);
                init_fd_pool(servers, fd_pool_len - 1);
            }
            if (check_mp_over(now_rest))
                return false;
            else if (need_cancel[i] == 0)
                send_new_req(servers, i, req);
            return true;
        }
        return true;
    }
    void send_new_req(StreamPtr (&servers)[path_num], int i /*server_number*/, const Request &req)
    {
        if (path_num != 2)
        {
            printf("[err] send_new_req does not be implemented for case path_num != 2");
            exit(-1);
        }
        auto t = now_rest[1 - i];
        if ((now_rest = decide_send_new_req(servers, now_rest[1 - i], i))[i] != 0)
        {
            printf("server %d send_new_req, new work allocation, server 0: %ld bytes, server 1: %ld bytes\n", i, now_rest[0], now_rest[1]);
            need_cancel[1 - i] = 1;
            has_read_header[i] = 0;
            works[1 - i] -= t - now_rest[1 - i];
            works[i] = now_rest[i];
            start_point[i] = start_point[1 - i] + works[1 - i];
            now_buffer_ptr[i] = now_buffer_ptr[1 - i] + now_rest[1 - i];
            char tmp_buf[100];
            sprintf(tmp_buf, "bytes=%ld-%ld\r\n", start_point[i], start_point[i] + works[i] - 1);
            auto r = req;
            r.headers.at(range_header) = tmp_buf;
            servers[i]->sendReqeust(servers[i]->fd, r, Body{});
            send_req_time[i] = std::chrono::system_clock::now();
            return;
        }
    }
};

int main(int argc, char **argv)
{
    int listenfd, *connfd;
    pthread_t tid;
    char hostname[max_line], port[max_line];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    /* Check command line args */
    if (argc > 3)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    if (argc == 3)
        mp = 0;
    listenfd = Open_listenfd(argv[1]);
    while (1)
    {
        clientlen = sizeof(clientaddr);
        connfd = (int *)malloc(sizeof(int));
        *connfd = Accept(listenfd, (sockaddr *)&clientaddr, &clientlen);

        Getnameinfo((sockaddr *)&clientaddr, clientlen, hostname, max_line,
                    port, max_line, 0);
        printf("Accepted connection from (%s:%s)\n", hostname, port);
        pthread_create(&tid, NULL, Thread, connfd);
    }
    Close(listenfd);
    return 0;
}

/* Thread routine */
void *Thread(void *vargp)
{
    int connfd = *((int *)vargp);
    pthread_detach(pthread_self());
    free(vargp);
    doit(connfd);
    return NULL;
}

void doit(int client_fd)
{
    //int endserver_fd;
    std::future<std::pair<int, int>> fu;
    auto clientStream = HttpStream::generateStream(client_fd);
    StreamPtr serverStream[path_num] = {HttpStream::generateStream(0), HttpStream::generateStream(0)};
    if (!clientStream)
    {
        printf("[Err] Connection failed getStream\n");
        return;
    }

    while (1)
    {
        Body req_body, reply_body;
        auto req_ptr = clientStream->getRequest(req_body);
        if (!req_ptr)
        { // client close the connection
            //printf("[Err] Connection failed at get reqeust.\n");
            break;
        }
        else
        {
            for (int i = 0; i < path_num; ++i)
                serverStream[i]->port = clientStream->port;
        }
        decltype(auto) req = req_ptr.value();
        if (req.method == connect_method)
        {
            do_relay(clientStream.get());
            break;
        }
        std::pair<uint64_t, uint64_t> byte_range{0, 0};
        std::optional<Response> response;
        auto mp_http_type = mp_http::not_use;
        if ((mp_http_type = need_mp_http(req, byte_range)) == mp_http::not_use)
        {
            response = send_normal_request(clientStream, serverStream, req, req_body, reply_body);
            if (!response)
            {
                printf("[Err] normal request failed at get response.\n");
                break;
            }
        }
        else
        {
            using namespace std::chrono;
            auto now = system_clock::now();
            printf("send_mp_request start\n");
            response = send_mp_request(clientStream, serverStream, req, req_body, reply_body, mp_http_type, byte_range, fu);
            if (!response)
            {
                printf("[Err] mp request failed at get response.\n");
                break;
            }
            printf("send_mp_request end, time spent %d ms\n", duration_cast<milliseconds>(system_clock::now() - now).count());
        }
        if (!clientStream->sendResponse(clientStream->fd, response.value(), reply_body))
            break;
        if (fu.valid())
        {
            auto ret = fu.get(); // blocking wait
            serverStream[ret.first]->fd.rio_fd = ret.second;
            if (ret.second)
                printf("reconnecting server %i succeed\n", ret.first);
        }
    }
    clientStream->close();
    serverStream[0]->close();
    serverStream[1]->close();
    // may be blocked because of destructor of std::future ?
}

std::optional<Response> send_normal_request(StreamPtr &client, StreamPtr (&servers)[path_num], const Request &client_req, Body &req_body, Body &reply_body)
{
    if (!(*servers[0].get()) && !(*servers[1].get()))
    {
        uint addr;
        servers[0]->fd.rio_fd = Open_clientfd(client->hostname.c_str(), std::to_string(client->port).c_str(), &servers[0]->latency, &addr);
        servers[0]->other_ip_addr = addr;
        if (servers[0]->fd.rio_fd < 0)
        {
            servers[0]->fd.rio_fd = 0;
            printf("[Err] Connection failed at open endserver_fd\n");
            return std::nullopt;
        }
    }
    HttpStream *server = *servers[0].get() ? servers[0].get() : servers[1].get();
    if (!server->sendReqeust(server->fd, client_req, req_body))
        return std::nullopt;

    using namespace std::chrono;

    auto now = system_clock::now();
    auto response = server->getResponse();
    server->latency = duration_cast<milliseconds>(system_clock::now() - now).count();

    if (!response)
        return std::nullopt;
    decltype(auto) r = response.value();
    if (r.headers.count(content_len_hdr) == 1)
    {
        auto leng = std::stoul(r.headers.at(content_len_hdr));
        now = system_clock::now();
        if (server->fd.rio_readnb(reply_body, leng) != (ssize_t)leng)
            return std::nullopt;
        auto delay = duration_cast<milliseconds>(system_clock::now() - now).count();
        if (delay > 0)
            server->bandwidth = leng / delay;
    }
    else
    { // connection close or transfer-encoding
        printf("[Error] Not implemented!.\n");
        return std::nullopt;
    }
    return response;
}

std::optional<Response> send_mp_request(StreamPtr &client, StreamPtr (&servers)[path_num], const Request &client_req,
                                        Body &req_body, Body &reply_body, mp_http type, const std::pair<uint64_t, uint64_t> &byte_range,
                                        std::future<std::pair<int, int>> &fu)
{
    std::optional<Response> response;
    if (type == mp_http::no_range_header)
    {
        printf("not implement no range header mp-req now\n");
        return std::nullopt;
    }
    Open_mp_clientfd(client->hostname.c_str(), std::to_string(client->port).c_str(), servers);
    if (choose_server(servers) || mp == 0)
        return send_normal_request(client, servers, client_req, req_body, reply_body);
    if (byte_range.second == 0)
    {
        printf("send_mp_request: byte_range.second == 0\n");
        exit(-1);
    }
    if (req_body.length != 0)
    {
        printf("send_mp_request: req_body.length != 0\n");
        return std::nullopt;
    }
    reply_body.content.reset(new uint8_t[byte_range.second - byte_range.first + 1]);
    reply_body.length = byte_range.second - byte_range.first + 1;
/*
<<<<<<< Updated upstream
    std::vector<uint64_t> works = dispatch_work(servers, byte_range.second - byte_range.first + 1);
    std::vector<uint64_t> tmp_works = works;
    printf("two path: %ld bytes and %ld bytes\n", works[0], works[1]);
    if ((int64_t)works[0] < 0 || (int64_t)works[1] < 0)
    {
        printf("\n\n!!!!!!!!!!!!err!!!!!!!!!!!!!!!!!!!!!\n\n");
        return std::nullopt;
    }
    
    uint8_t *finished_ptr[path_num];
    for (int i = 0; i < path_num; ++i)
    {
        if (i == 0)
            finished_ptr[i] = reply_body.content.get();
        else
            finished_ptr[i] = finished_ptr[i - 1] + works[i - 1];
    }

    bool has_read_header[path_num] = {0};
    Request req_0 = client_req, req_1 = client_req;
    pollfd fd_pool[path_num];
    for (int i = 0; i < path_num; ++i)
    {
        fd_pool[i].fd = servers[i]->fd.rio_fd;
        fd_pool[i].events = POLLIN;
        fd_pool[i].revents = 0;
    }

    char tmp_buffer[100];
    sprintf(tmp_buffer, "bytes=%ld-%ld\r\n", byte_range.first, byte_range.first + works[0] - 1);
    req_0.headers.at(range_header) = string(tmp_buffer);
    sprintf(tmp_buffer, "bytes=%ld-%ld\r\n", byte_range.first + works[0], byte_range.second);
    req_1.headers.at(range_header) = string(tmp_buffer);

    if (!servers[0]->sendReqeust(servers[0]->fd, req_0, req_body))
    {
        printf("send_mp_request: servers[0] sent req and failed\n");
        return std::nullopt;
    }
    if (!servers[1]->sendReqeust(servers[1]->fd, req_1, req_body))
=======*/
    workState mp_work(byte_range, servers, reply_body, fu);
    if (!mp_work.send_req(client_req, servers))
//>>>>>>> Stashed changes
    {
        printf("send_mp_request: first mp_req failed\n");
        return std::nullopt;
    }
    using namespace std::chrono;

    while (poll(mp_work.fd_pool.get(), mp_work.fd_pool_len, -1) >= 0)
    {
        for (int j = 0; j < mp_work.fd_pool_len; ++j)
        {
            int i = find_server_number(mp_work.fd_pool[j].fd, servers);
            if (mp_work.fd_pool[i].revents & POLLIN)
            {
                if (mp_work.has_read_header[i] == 0)
                {
                    response = servers[i]->getResponse();
                    mp_work.get_header_time[i] = system_clock::now();
                    update_latency(duration_cast<milliseconds>(mp_work.get_header_time[i] - mp_work.send_req_time[i]).count(), servers[i].get());
                    if (!response || !check_mp_response(response.value()))
                    {
                        printf("send_mp_request: response header check failed\n");
                        return std::nullopt;
                    }
                    mp_work.has_read_header[i] = 1;
                    auto buf_bytes = servers[i]->fd.rio_get_rest();
                    assert(servers[i]->fd.rio_read(mp_work.now_buffer_ptr[i], buf_bytes) == buf_bytes);
                    mp_work.now_buffer_ptr[i] += buf_bytes;
                    mp_work.now_rest[i] -= buf_bytes;
                    if (!mp_work.decide_continue(servers, i, client_req))
                        goto over;
                }
                else
                {
                    auto rd_cnt = read(servers[i]->fd.rio_fd, mp_work.now_buffer_ptr[i], mp_work.now_rest[i]);
                    if (rd_cnt <= 0)
                    {
                        printf("send_mp_request: rd_cnt = %ld\n", rd_cnt);
                        //return std::nullopt;
                    }
                    mp_work.now_buffer_ptr[i] += rd_cnt;
                    if (mp_work.now_rest[i] < rd_cnt)
                    {
                        printf("send_mp_request: works[i] < rd_cnt\n");
                        return std::nullopt;
                    }
                    mp_work.now_rest[i] -= rd_cnt;
                    if (!mp_work.decide_continue(servers, i, client_req))
                        goto over;
                }
            }
            else if (mp_work.fd_pool[i].revents & POLLHUP)
            {
                servers[i]->close();
                if (mp_work.now_rest[i] != 0)
                {
                    printf("send_mp_request: early close of server\n");
                    return std::nullopt;
                }
            }
        }
        mp_work.check_future(servers, client_req);
    }
over:
    Response &r = response.value();
    char tmp_buffer[100];
    uint64_t a, b, c;
    if (sscanf(r.headers[content_range_header].c_str(), "bytes %ld-%ld/%ld", &a, &b, &c) != 3)
    {
        printf("send_mp_request: sscanf failed\n");
        return std::nullopt;
    }
    sprintf(tmp_buffer, "bytes %ld-%ld/%ld\r\n", byte_range.first, byte_range.second, c);
    r.headers[content_range_header] = tmp_buffer;
    sprintf(tmp_buffer, "%ld\r\n", byte_range.second - byte_range.first + 1);
    r.headers[content_len_hdr] = tmp_buffer;
    return response;
}

void do_relay(HttpStream *client)
{
    uint latency, t;
    int byte_count;
    auto server_fd = Open_clientfd(client->hostname.c_str(), std::to_string(client->port).c_str(), &latency, &t);
    if (server_fd < 0)
        return;
    client->fd.rio_writen(connect_response.c_str(), connect_response.size()); // send connect response

    pollfd fd_pool[2]; // fd_pool[0] -> client, fd_pool[1] -> server
    fd_pool[0].fd = client->fd.rio_fd;
    fd_pool[0].events = POLLIN;
    fd_pool[0].revents = 0;
    fd_pool[1].fd = server_fd;
    fd_pool[1].events = POLLIN;
    fd_pool[1].revents = 0;
    char buf[max_line];

    while (poll(fd_pool, 2, -1) >= 0) // -1 means no timeout
    {
        for (int i = 0; i < 2; ++i)
        {
            if (fd_pool[i].revents & POLLIN)
            {
                byte_count = read(fd_pool[i].fd, buf, max_line);
                if (writen(fd_pool[1 - i].fd, buf, byte_count) < 0)
                    break;
            }
        }
        if (!(fd_pool[0].revents & POLLIN) && (fd_pool[0].revents & POLLHUP) &&
            !(fd_pool[1].revents & POLLIN) && (fd_pool[1].revents & POLLHUP))
            break;
    }
    Close(server_fd);
}

void parse_uri(char *uri, char *hostname, char *path, int *port)
{
    *port = 80;
    char *pos1 = strstr(uri, "//");
    if (pos1 == NULL)
        pos1 = uri;
    else
        pos1 += 2;
    char *pos2 = strstr(pos1, ":");
    if (pos2 != NULL)
    {
        *pos2 = '\0';
        strncpy(hostname, pos1, max_line);
        sscanf(pos2 + 1, "%d%s", port, path);
        *pos2 = ':';
    }
    else
    {
        pos2 = strstr(pos1, "/"); //pos2 /hub/index.html
        if (pos2 == NULL)
        { /*pos1 www.cmu.edu*/
            strncpy(hostname, pos1, max_line);
            strcpy(path, "");
            return;
        }
        *pos2 = '\0';
        strncpy(hostname, pos1, max_line);
        *pos2 = '/';
        strncpy(path, pos2, max_line);
    }
}

mp_http need_mp_http(const Request &req, std::pair<uint64_t, uint64_t> &range)
{
    /*if (mp == 0)
        return mp_http::not_use;*/
    if (req.method.compare("GET") != 0)
        return mp_http::not_use;
    if (req.headers.count(range_header) == 1)
    {
        int ret = sscanf(req.headers.at(range_header).c_str(), "bytes=%ld-%ld", &range.first, &range.second);
        if (ret < 2 || range.second - range.first < threshold)
            return mp_http::not_use;
        return mp_http::has_range_header;
    }
    else
    {
        auto index = req.url.find_last_of('.');
        if (index != string::npos && req.url.substr(index).compare(".m4s") == 0)
            return mp_http::no_range_header;
        return mp_http::not_use;
    }
}
