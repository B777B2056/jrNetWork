#include "Multiplexer.h"
#include "Event.h"
#include "Log.h"
#include <unistd.h>
#include <cstring>

namespace jrNetWork
{
	constexpr int eEventListInitSize = 8;
	constexpr int eEventListMaxSize = 1024;

	_MultiplexerBase::_MultiplexerBase()
		: _waitEvN(0)
		, _listenSock(0)
		, _activateNativeEvents(eEventListInitSize)
	{

	}

	MultiplexerIterator::MultiplexerIterator(_MultiplexerBase& mp, std::size_t i)
		: _idx(i)
		, _innerEv(mp._activateNativeEvents[i])
		, _mp{mp}
	{

	}

	void MultiplexerIterator::_checkListener()
	{
		if (_mp._listenSock == _mp._activateNativeEvents[_idx].data.fd)
		{
			_innerEv.type = EventType::LISTEN;
		}
	}

	Event& MultiplexerIterator::operator*()
	{
		_checkListener();
		return _innerEv;
	}

	Event* MultiplexerIterator::operator->()
	{
		_checkListener();
		return &_innerEv;
	}

	MultiplexerIterator& MultiplexerIterator::operator++()
	{
		_innerEv = _mp._activateNativeEvents[++_idx];
		return *this;
	}

	MultiplexerIterator MultiplexerIterator::operator++(int)
	{
		MultiplexerIterator tmp = *this;
		_innerEv = _mp._activateNativeEvents[++_idx];
		return tmp;
	}

	MultiplexerIterator& MultiplexerIterator::operator--()
	{
		_innerEv = _mp._activateNativeEvents[--_idx];
		return *this;
	}

	MultiplexerIterator MultiplexerIterator::operator--(int)
	{
		MultiplexerIterator tmp = *this;
		_innerEv = _mp._activateNativeEvents[--_idx];
		return tmp;
	}

	bool MultiplexerIterator::operator==(const MultiplexerIterator& rhs)
	{
		return _idx == rhs._idx;
	}

	bool MultiplexerIterator::operator!=(const MultiplexerIterator& rhs)
	{
		return !(*this == rhs);
	}

	namespace Poll
	{
		
	}

	namespace Epoll
	{
		Multiplexer::Multiplexer()
			: _MultiplexerBase()
			, _epollfd(::epoll_create(eEventListInitSize))
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

		Multiplexer::iterator Multiplexer::begin()
		{
			return Multiplexer::iterator(*this, 0);
		}

		Multiplexer::iterator Multiplexer::end()
		{
			return Multiplexer::iterator(*this, _waitEvN);
		}

		void Multiplexer::registEvent(const Event& ev)
		{
			_changeEvent(ev, EPOLL_CTL_ADD);
		}

		void Multiplexer::unregistEvent(const Event& ev)
		{
			_changeEvent(ev, EPOLL_CTL_DEL);
		}

		void Multiplexer::wait(int timeoutMs)
		{
			int n = ::epoll_wait(_epollfd, &_activateNativeEvents[0],
						   static_cast<int>(_activateNativeEvents.size()), timeoutMs);
			if (-1 == n)
			{
				_waitEvN = 0;
				// 出错
				if (errno != EINTR)
				{
					LOGWARN() << "Epoll wait failed, " << ::strerror(errno) << std::endl;
				}
			}
			else if (n > 0)
			{
				_waitEvN = n;
				// 本次通知达到预设的最大事件数，说明需要进行扩容
				if (n == static_cast<int>(_activateNativeEvents.size()))
				{
					_activateNativeEvents.resize(_activateNativeEvents.size() * 2);
				}
			}
			else
			{
				_waitEvN = 0;
				// 超时
				LOGNOTICE() << "Timeout." << std::endl;
			}
		}
	}
}
