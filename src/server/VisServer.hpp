#pragma once

#include "editor/Editor.hpp"

#include <atomic>
#include <thread>

// Forward-declare to avoid pulling httplib into every translation unit
namespace httplib { class Server; }

class VisServer {
public:
    explicit VisServer(Editor& editor, int port = 8080);
    ~VisServer();

    void start();
    void stop();

private:
    void serveLoop();
    std::string buildJson() const;
    std::string buildHtml() const;

    Editor& editor_;
    int port_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    httplib::Server* svr_ = nullptr;
};
