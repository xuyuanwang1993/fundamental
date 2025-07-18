#pragma once
#include "network/network.hpp"
#include "rpc/proxy/rpc_forward_connection.hpp"
namespace network
{
namespace proxy
{
class protocal_pipe_connection : public rpc_forward_connection {

public:
    template <typename... Args>
    static decltype(auto) make_shared(Args&&... args) {
        return std::make_shared<protocal_pipe_connection>(std::forward<Args>(args)...);
    }
    explicit protocal_pipe_connection(std::shared_ptr<rpc_service::connection> ref_connection,
                                      rpc_client_forward_config forward_config,
                                      std::string pre_read_data = "");

protected:
    void process_protocal() override;

protected:
    rpc_client_forward_config forward_config_;
};
} // namespace proxy
} // namespace network