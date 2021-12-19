#include "dispatch.hpp"

namespace jrNetWork {
    int jrNetWork::UnifiedEventSource::uesfd[2];

    std::string error_handle(std::string msg) { return msg + strerror(errno); }

    void UnifiedEventSource::ues_transfer(int sig) {
        write(uesfd[1], reinterpret_cast<char*>(&sig), 1);
    }

    UnifiedEventSource::UnifiedEventSource() {
        if(-1 == pipe(uesfd)) {
            std::string msg = jrNetWork::error_handle("Unified Event Source pipe failed: ");
            LOG(Logger::Level::FATAL, msg);
            throw msg;
        }
        // Set ues fd write non-blocking
        int flag = fcntl(uesfd[1], F_GETFL);
        flag |= O_NONBLOCK;
        fcntl(uesfd[1], F_SETFL, flag);
        // Alarm-time bind
        signal(SIGALRM, ues_transfer);
        // SIG sig bind
        signal(SIGINT, ues_transfer);
        signal(SIGTERM, ues_transfer);
        signal(SIGPIPE, ues_transfer);
    }

    UnifiedEventSource::~UnifiedEventSource() {
        ::close(uesfd[0]);
        ::close(uesfd[1]);
    }

    bool UnifiedEventSource::handle() {
        /* Handle signals */
        char sig[32];
        memset(sig, 0, sizeof(sig));
        int num = read(uesfd[0], sig, sizeof(sig));
        if(num <= 0)
            return false;
        for(int i = 0; i < num; ++i) {
            switch(sig[i]) {
                case SIGALRM:
                    return true;    // timeout task
                    LOG(Logger::Level::WARNING, "Client connection timeout");
                    break;
                case SIGTERM:
                case SIGINT:    // Stop server
                    LOG(Logger::Level::FATAL, "Server interrupt by system signal");
                    exit(1);
                case SIGPIPE:
                    break;
            }
        }
        return false;
    }
}
