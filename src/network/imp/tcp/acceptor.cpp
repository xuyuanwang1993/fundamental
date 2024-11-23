#include "acceptor.h"
#include "fundamental/basic/log.h"
#include "network/core/io-channel.h"
#include "network/core/platform_internal.h"
namespace io
{
Acceptor::Acceptor(TaskScheduler* pTaskSchedulerRef, const std::string& service) :
pScheduler(pTaskSchedulerRef),
service_name(service)
{
}

bool Acceptor::StartListen(uint32_t max_pending_size)
{
    bool ret = false;
    do
    {
        if (service_name.empty())
        {
            FERR("need a valid service name");
            break;
        }
        bool expected = false;
        if (!listenning.compare_exchange_strong(expected, true))
        {
            FWARN("accepltor is listening");
            return true;
        }
        struct addrinfo addr_guess;
        memset(&addr_guess, 0, sizeof(addr_guess));
        addr_guess.ai_family   = AF_UNSPEC;
        addr_guess.ai_flags    = AI_PASSIVE;
        addr_guess.ai_socktype = SOCK_STREAM;
        addr_guess.ai_protocol = IPPROTO_TCP;
        struct addrinfo* server_addr;
        auto ret = getaddrinfo(nullptr, service_name.c_str(), &addr_guess, &server_addr);
        if (ret != 0)
        {
            FERR("getaddrinfo for {} failed! {}", service_name.c_str(), gai_strerror(ret));
            break;
        }
        auto ptr = server_addr;
        for (; ptr; ptr = ptr->ai_next)
        {
            auto fd = ::socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
            if (fd < 0)
            {
                FERR("{} Create socket failed!{}", service_name.c_str(), strerror(platform::getErrno()));
                continue;
            }
            NETWORK_UTIL::set_reuse_addr(fd);
            if (ptr->ai_family == AF_INET6)
            { // otherwise binding call will be failed with the message "address is already in used"
                NETWORK_UTIL::sock_set_v6only(fd);
            }
            if ((bind(fd, ptr->ai_addr, ptr->ai_addrlen) == 0) &&
                (listen(fd, max_pending_size) == 0))
            {
                auto channel = pScheduler->AddChannel(fd);
                channel->bytesReady.Connect([this, fd]() {
                    this->OnAccept(fd);
                });
                channel->EnableReading();
                channel->Sync();
                server_channel_list.push_back(channel);
                FDEBUG("init tcp server [{}:{}] success", NETWORK_UTIL::get_local_ip(fd).c_str(), NETWORK_UTIL::get_local_port(fd));
            }
            else
            {
                NETWORK_UTIL::close_socket(fd);
                FERR("{} bind or listen failed!{}", service_name.c_str(), strerror(platform::getErrno()));
            }
        }
        freeaddrinfo(server_addr);
        ret = !server_channel_list.empty();
        if (!ret)
            listenning.exchange(false);
    } while (0);
    return ret;
}

void Acceptor::StopListen()
{
    for (auto i : server_channel_list)
    {
        i->Stop();
        NETWORK_UTIL::close_socket(i->GetFd());
    }
    server_channel_list.clear();
}

std::string Acceptor::ServiceName() const
{
    return service_name;
}

Acceptor::~Acceptor()
{
}

void Acceptor::OnAccept(int fd)
{
    struct sockaddr_storage addr;
    socklen_t len = sizeof(addr);
    SOCKET ret    = ::accept(fd, reinterpret_cast<sockaddr*>(&addr), &len);
    if (ret < 0)
    {
        FERR("{} accept failed!{}", service_name.c_str(), strerror(platform::getErrno()));
    }
    NETWORK_UTIL::set_tcp_keepalive(ret, true);
    NewConnectionComing(ret);
}

} // namespace io