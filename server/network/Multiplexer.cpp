#include "Multiplexer.h"
#include "Event.h"
#include "Log.h"
#include <unistd.h>
#include <cstring>

namespace jrNetWork
{
	constexpr int eEventListInitSize = 8;
	constexpr int eEventListMaxSize = 1024;

	namespace Epoll
	{
		Multiplexer::Multiplexer()
			: _epollfd(::epoll_create(eEventListInitSize))
			, _listenSock(0)
			, _activateNativeEvents(eEventListInitSize)
		{
			if (-1 == _epollfd)
			{
				LOGFATAL() << "Epoll create failed, " << ::strerror(errno) << std::endl;
			}
		}

		Multiplexer::~Multiplexer()
		{
			if (-1 == ::close(_epollfd))
			{
				LOGWARN() << "Epoll closed failed: " << ::strerror(errno) << std::endl;
			}
		}

		void Multiplexer::_changeEvent(const Event& ev, int op)
		{
			_NativeEvent ne;
			ne.data.fd = ev.id;
			switch (ev.type)
			{
			case EventType::LISTEN:
				_listenSock = ev.id;
			case EventType::READ:
			case EventType::SIGNAL:
				ne.events = EPOLLIN;
				break;
			case EventType::WRITE:
				ne.events = EPOLLOUT;
				break;
			default:
				break;
			}
			ne.events |= EPOLLET;
			if (-1 == ::epoll_ctl(_epollfd, op, ne.data.fd, &ne))
			{
				LOGWARN() << "Event regist/unregist failed: " << ::strerror(errno) << std::endl;
			}
		}

		void Multiplexer::registEvent(const Event& ev)
		{
			_changeEvent(ev, EPOLL_CTL_ADD);
		}

		void Multiplexer::unregistEvent(const Event& ev)
		{
			_changeEvent(ev, EPOLL_CTL_DEL);
		}

		void Multiplexer::wait(std::deque<Event>& activateEvents, int timeoutMs)
		{
			int n = ::epoll_wait(_epollfd, &_activateNativeEvents[0],
						   static_cast<int>(_activateNativeEvents.size()), timeoutMs);
			if (-1 == n)
			{
				// 出错
				if (errno != EINTR)
				{
					LOGWARN() << "Epoll wait failed, " << ::strerror(errno) << std::endl;
				}
			}
			else if (n > 0)
			{
				// 填充EventLoop激活事件队列
				for (int idx = 0; idx < n; ++idx)
				{
					activateEvents.emplace_back(_activateNativeEvents[idx]);
					if (_listenSock == _activateNativeEvents[idx].data.fd)
					{
						activateEvents.back().type = EventType::LISTEN;
					}
				}
				// 本次通知达到预设的最大事件数，说明需要进行扩容
				if (n == static_cast<int>(_activateNativeEvents.size()))
				{
					_activateNativeEvents.resize(_activateNativeEvents.size() * 2);
				}
			}
			else
			{
				// 超时
				LOGNOTICE() << "Timeout." << std::endl;
			}
		}
	}
}
