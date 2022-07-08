#include "Ues.h"
#include "Log.h"
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <sys/socket.h>

namespace jrNetWork
{
    int _UnifiedEventSource::_uesfd[2] = {0, 0};

    // Init ues
    _UnifiedEventSource::_UnifiedEventSource()
    {
        if (-1 == ::socketpair(AF_UNIX, SOCK_STREAM, 0, _uesfd))
        {
            LOGFATAL() << "UES endpoint init failed: " << ::strerror(errno) << std::endl;
        }
        // Set ues fd write non-blocking
        ::fcntl(_uesfd[1], F_SETFL, ::fcntl(_uesfd[1], F_GETFL) | O_NONBLOCK);
    }

    // Close fd
    _UnifiedEventSource::~_UnifiedEventSource()
    {
        ::close(_uesfd[1]);
        ::close(_uesfd[0]);
    }

    // Wrire signal to uesfd
    void _UnifiedEventSource::signalTransfer(int sig)
    {
        LOGNOTICE() << "Catch signal, signal " << sig << std::endl;
        ::write(_uesfd[1], reinterpret_cast<char*>(&sig), 1);
    }

    // Bind signal
    void _UnifiedEventSource::bindSignal(int sig)
    {
        struct sigaction act;
        ::memset(&act, 0, sizeof(struct sigaction));
        act.sa_handler = signalTransfer;
        ::sigemptyset(&act.sa_mask);
        ::sigaddset(&act.sa_mask, sig);
        act.sa_flags |= SA_RESTART;
        if (-1 == ::sigaction(sig, &act, nullptr))
        {
            LOGFATAL() << "Signal redirect failed: " << ::strerror(errno) << std::endl;
        }
        else
        {
            LOGNOTICE() << "Signal redirect success, " << sig << std::endl;
        }
    }

    bool _UnifiedEventSource::handleSignals(std::unordered_map<int, std::function<void()> >& sigHandlerTbl)
    {
        char sig[32];
        bool isTimeout = false;
        ::memset(sig, 0, sizeof(sig));
        int num = ::read(_uesfd[0], sig, sizeof(sig));
        if (num <= 0)
        {
            return false;
        }
        for (int i = 0; i < num; ++i)
        {
            if (sig[i] == SIGALRM)
            {
                isTimeout = true;
            }
            else
            {
                sigHandlerTbl[sig[i]]();
            }
        }
        return isTimeout;
    }
}
