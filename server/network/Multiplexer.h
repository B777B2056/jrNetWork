#pragma once

#include "Event.h"
#include <vector>
#include <iterator>
#include <sys/poll.h>
#include <sys/epoll.h>

namespace jrNetWork
{
	class MultiplexerIterator;

    class _MultiplexerBase
    {
		friend class MultiplexerIterator;
	protected:
		int _waitEvN;
		int _listenSock;
		std::vector<_NativeEvent> _activateNativeEvents;
		virtual void _changeEvent(const Event& event, int op);

	public:
		using iterator = MultiplexerIterator;

		_MultiplexerBase();
        virtual ~_MultiplexerBase() {}
		virtual iterator begin() = 0;
		virtual iterator end() = 0;
        virtual void registEvent(const Event&) = 0;
        virtual void unregistEvent(const Event&) = 0;
        virtual void wait(int t = -1) = 0;
    };

	class MultiplexerIterator : public std::iterator<std::bidirectional_iterator_tag, Event>
	{
	private:
		std::size_t _idx;
		Event _innerEv;
		_MultiplexerBase& _mp;

		void _checkListener();

	public:
		MultiplexerIterator(_MultiplexerBase& mp, std::size_t i);

		Event& operator*();
		Event* operator->();

		MultiplexerIterator& operator++();
		MultiplexerIterator operator++(int);

		MultiplexerIterator& operator--();
		MultiplexerIterator operator--(int);

		bool operator==(const MultiplexerIterator& rhs);
		bool operator!=(const MultiplexerIterator& rhs);
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
			void _changeEvent(const Event& event, int op) override;

		public:
			Multiplexer();
			~Multiplexer();

			iterator begin() override;
			iterator end() override;

			void registEvent(const Event& event) override;
			void unregistEvent(const Event& event) override;
			void wait(int timeoutMs = -1) override;
		};
    }
}
