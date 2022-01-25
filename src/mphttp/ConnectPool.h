#ifndef CONNECTPOOL_H
#define CONNECTPOOL_H

#include <list>

#include "HttpClient.h"
#include "MpHttpClient.h"

struct EventLoop;

struct ConnectPool {
   public:
    ConnectPool(struct EventLoop* loop, struct MpHttpClient* mp,
                struct sockaddr_in addr_in)
        : loop_(loop), mp_(mp), addr_in_(addr_in) {
        CreateNewHttpClient();
    }

    ~ConnectPool() {
        // release all connection in pool
        for (auto iter = client_pool_.begin(); iter != client_pool_.end();) {
            struct HttpClient* client = *iter;
            close(client->sock_fd);
            delete client;
            iter = client_pool_.erase(iter);
        }
    }

    struct HttpClient* GetHttpClient() {
        MPHTTP_ASSERT(client_pool_.size() > 0, "Connection Pool is empty!");

        std::list<struct HttpClient*>::iterator iter = client_pool_.begin();

        struct HttpClient* chosen_client = nullptr;

        bool found = false;
        for (; iter != client_pool_.end(); iter++) {
            chosen_client = *iter;
            if (chosen_client->pool_state_ == kIdleConnected) {
                client_pool_.erase(iter);
                found = true;
                break;
            }
        }

        if (!found) {
            MPHTTP_LOG(info, "ConnectPool : no available HttpClient");
            iter = client_pool_.begin();
            chosen_client = *iter;
            client_pool_.erase(iter);
        }

        CreateNewHttpClient();
        return chosen_client;
    }

    void CreateNewHttpClient() {
        while (client_pool_.size() < pool_size_) {
            // FIXME : multi path task buffer
            // FIXME : start and end
            struct HttpClient* client =
                new HttpClient(loop_, mp_, nullptr, addr_in_, 0, 0);
            client->pool_state_ = kIdleConnecting;
            client_pool_.push_back(client);
        }
    }

    size_t pool_size_{3};
    struct EventLoop* loop_;
    struct MpHttpClient* mp_;
    struct sockaddr_in addr_in_;
    std::list<struct HttpClient*> client_pool_;
};

#endif  // CONNECTPOOL_H