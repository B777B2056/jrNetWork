#pragma once

#include "Event.h"
#include <vector>
#include <sys/epoll.h>

namespace jrNetWork
{
    struct _MultiplexerBase 
    {
        virtual ~_MultiplexerBase() {}
        virtual void registEvent(const Event&) = 0;
        virtual void unregistEvent(const Event&) = 0;
        virtual void wait(std::deque<Event>&, int t = -1) = 0;
    };

	namespace Select
	{

	}

	namespace Poll
	{

	}

    namespace Epoll
    {
		class Multiplexer : public _MultiplexerBase
		{
		private:
			int _epollfd;
			int _listenSock;
			std::vector<_NativeEvent> _activateNativeEvents;

			void _changeEvent(const Event& event, int op);

		public:
			Multiplexer();
			~Multiplexer();

			void registEvent(const Event& event) override;
			void unregistEvent(const Event& event) override;
			void wait(std::deque<Event>& activateEvents, int timeoutMs = -1) override;
		};
    }
}
