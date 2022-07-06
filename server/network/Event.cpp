#include "Event.h"

namespace jrNetWork
{
	Event::Event(const _NativeEvent& ne)
	{
		id = ne.data.fd;
		if ((ne.events & EPOLLHUP) || (ne.events & EPOLLERR))
		{
			type = EventType::ConnClosed;
		}
		else if (ne.events & EPOLLIN)
		{
			type = EventType::READ;
		}
		else if (ne.events & EPOLLOUT)
		{
			type = EventType::WRITE;
		} 
	}

	Event::Event(_NativeEvent&& ne)
	{
		id = ne.data.fd;
		if ((ne.events & EPOLLHUP) || (ne.events & EPOLLERR))
		{
			type = EventType::ConnClosed;
		}
		else if (ne.events & EPOLLIN)
		{
			type = EventType::READ;
		}
		else if (ne.events & EPOLLOUT)
		{
			type = EventType::WRITE;
		}
	}

	Event& Event::operator=(const _NativeEvent& ne)
	{
		id = ne.data.fd;
		if ((ne.events & EPOLLHUP) || (ne.events & EPOLLERR))
		{
			type = EventType::ConnClosed;
		}
		else if (ne.events & EPOLLIN)
		{
			type = EventType::READ;
		} 
		else if (ne.events & EPOLLOUT)
		{
			type = EventType::WRITE;
		}
		return *this;
	}

	Event& Event::operator=(_NativeEvent&& ne)
	{
		id = ne.data.fd;
		if ((ne.events & EPOLLHUP) || (ne.events & EPOLLERR))
		{
			type = EventType::ConnClosed;
		}
		else if (ne.events & EPOLLIN)
		{
			type = EventType::READ;
		}
		else if (ne.events & EPOLLOUT)
		{
			type = EventType::WRITE;
		}
		return *this;
	}

	bool Event::operator==(const Event& rhs)
	{
		return (id == rhs.id) && (type == rhs.type);
	}

	bool Event::operator!=(const Event& rhs)
	{
		return !(*this == rhs);
	}
}
