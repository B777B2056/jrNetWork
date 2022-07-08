#pragma once

#include <functional>
#include <unordered_map>

namespace jrNetWork
{
    struct _UnifiedEventSource
    {
        static int _uesfd[2];

        _UnifiedEventSource();
        ~_UnifiedEventSource();

        static void signalTransfer(int sig);
        static void bindSignal(int sig);
        static bool handleSignals(std::unordered_map<int, std::function<void()> >& sigHandlerTbl);
    };
}
