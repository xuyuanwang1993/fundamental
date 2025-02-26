

#include "fundamental/application/application.hpp"
#include "fundamental/basic/log.h"
#include "fundamental/delay_queue/delay_queue.h"
#include "network/server/io_context_pool.hpp"
#include <cstdlib>
#include <iostream>
#include <memory>
#include <random>
#include <utility>
using namespace Fundamental;
struct ApplicationImp : public ApplicationInterface {
    bool Load(int argc, char** argv) override;
    bool Init() override;
    void Exit() override;
    std::shared_ptr<asio::steady_timer> timer;
};
int main(int argc, char* argv[]) {
    do {
        auto& app = Fundamental::Application::Instance();
        app.OverlayApplication(std::make_shared<ApplicationImp>());
        if (!app.Load(argc, argv)) break;
        if (!app.Init()) break;
        app.Loop();
        return EXIT_SUCCESS;
    } while (0);
    return EXIT_FAILURE;
}

bool ApplicationImp::Load(int argc, char** argv) {
    FINFO("imp load");
    return true;
}

bool ApplicationImp::Init() {
    FINFO("imp init");
    network::io_context_pool::s_excutorNums = 3;
    network::io_context_pool::Instance().start();
    Fundamental::Application::Instance().exitStarted.Connect([&]() { network::io_context_pool::Instance().stop(); });
    network::io_context_pool::Instance().notify_sys_signal.Connect(
        [](std::error_code code, std::int32_t signo) { Fundamental::Application::Instance().Exit(); });
    timer = std::make_shared<asio::steady_timer>(network::io_context_pool::Instance().get_io_context(),
                                                 asio::chrono::seconds(10));
    timer->async_wait([](std::error_code code) {
        if (!code) {
            FWARN("quit");
            Application::Instance().Exit();
        } else {
            FWARN("abort for {}", code.message());
        }
    });
    return true;
}

void ApplicationImp::Exit() {
    FINFO("imp exit");
}
