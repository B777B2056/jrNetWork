#pragma once

#include <memory>
#include <cstring>
#include <cstdint>
#include <unordered_map>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include "Event.h"
#include "Multiplexer.h"
#include "Socket.h"
#include "Timer.h"
#include "ThreadPool.h"
#include "Log.h"
#include "Ues.h"

namespace jrNetWork
{
    template<class SocketType>
	class EventLoop
	{
    private:
        using CltPtrType = std::shared_ptr<SocketType>;
        using TimeoutCallbackType = std::function<void(CltPtrType)>;
        using IOCallbackType = std::function<void(CltPtrType)>;

    private:
        SocketType socket;
        std::unique_ptr<_MultiplexerBase> _multiplexer;
        std::unordered_map<int, CltPtrType> _idSocketTbl;
        _UnifiedEventSource _ues;
        /* Event handlers */
        IOCallbackType _readEvHandler;
        IOCallbackType _writeEvHandler;
        TimeoutCallbackType _timeoutCallback;
        /* Timer */
        TimerContainer<SocketType> _timer;
        /* Thread pool */
        ThreadPool _threadPool;
        /* Signal-Handler table */
        std::unordered_map<int, std::function<void()> > _sigHandlerTbl;

        /* Socket init */
        void _socketInit(std::uint16_t port)
        {
            /* Bind ip address and port */
            socket.bind(port);
            /* Listen target port */
            socket.listen();
            /* Regist listen event */
            Event ev;
            ev.id = socket._id;
            ev.type = EventType::LISTEN;
            _multiplexer->registEvent(ev);
            /* Regist signal event */
            ev.id = _UnifiedEventSource::_uesfd[0];
            ev.type = EventType::SIGNAL;
            _multiplexer->registEvent(ev);
        }

        /* Do Accept */
        void _doAccept()
        {
            CltPtrType cltPtr = socket.accept();
            if (cltPtr)
            {
                Event readEv;
                readEv.id = cltPtr->_id;
                readEv.type = EventType::READ;
                _multiplexer->registEvent(readEv);
                _idSocketTbl[cltPtr->_id] = cltPtr;
                _timer.addTask(cltPtr);
            }
            else
            {
                LOGWARN() << "Accept failed, " << ::strerror(errno) << std::endl;
            }
        }

        /* Event handler */
        void _handleEvent(std::uint16_t timeoutMs, Event& ev)
        {
            if (ev.type == EventType::LISTEN)
            {
                _doAccept();
            }
            if (ev.type == EventType::READ)
            {
                if (ev.id == _UnifiedEventSource::_uesfd[0])
                {
                    ev.type == EventType::SIGNAL;
                }
                else
                {
                    _threadPool.addTask([this, ev]()->void
                    {
                        // Execute user-specified logic
                        _readEvHandler(_idSocketTbl[ev.id]);
                        // If the data has not been sent at one time,
                        // it will be pushed into the buffer (completed by TCP::Socket),
                        // and then register the EPOLLOUT event to wait for the next sending.
                        if (!_idSocketTbl[ev.id]->isSendAll())
                        {
                            Event writeEv;
                            writeEv.id = ev.id;
                            writeEv.type = EventType::WRITE;
                            _multiplexer->registEvent(writeEv);
                        }
                    });
                }
            }
            if (ev.type == EventType::WRITE)
            {
                _threadPool.addTask([this, ev]()->void
                {
                    // Send rest data in buf
                    if (_sendRestBuf(_idSocketTbl[ev.id]))
                    {
                        // When all sended, execute user-specified logic
                        _writeEvHandler(_idSocketTbl[ev.id]);
                    }
                });
            }
            if (ev.type == EventType::ConnClosed)
            {
                ::close(ev.id);
            }
            if (ev.type == EventType::SIGNAL)
            {
                if (_UnifiedEventSource::handleSignals(_sigHandlerTbl))
                {
                    ev.type = EventType::Timeout;
                }
            }
            if (ev.type == EventType::Timeout)
            {
                _threadPool.addTask([this, ev]()->void
                {
                    // Update the timer container, handle timeout clients
                    _timer.tick(_timeoutCallback);  
                });
            }
        }

        /* Send data in buffer */
        bool _sendRestBuf(CltPtrType cltPtr)
        {
            if (!cltPtr->isSendAll())
            {
                /* Send data in buffer */
                std::string pre_data = cltPtr->_sendBuffer.getData();
                uint pre_sent_size = cltPtr->send(pre_data);
                /* A part is sent, and the rest is restored to the buffer */
                if (pre_sent_size < pre_data.length())
                {
                    cltPtr->_sendBuffer.append(pre_data.begin(), pre_data.end());
                    return false;
                }
                else
                {
                    Event writeEv;
                    writeEv.id = cltPtr->_id;
                    writeEv.type = EventType::WRITE;
                    _multiplexer->unregistEvent(writeEv);
                    return true;
                }
            }
            else
            {
                return true;
            }
        }

        /* Handler set helper */
        template<typename F, typename... Args>
        IOCallbackType _handlerSetHelper(F&& handler, Args&&... args)
        {
            auto handlerBinder = std::bind(std::forward<F>(handler), std::forward<Args>(args)..., std::placeholders::_1);
            return [handlerBinder](CltPtrType cltPtr)->void
            {
                if (cltPtr)
                {
                    handlerBinder(cltPtr);
                }            
            };
        }

    public:
        /* Init thread pool and IO model */
        EventLoop(std::uint16_t port, std::uint16_t maxPoolSize = std::thread::hardware_concurrency())
            : _multiplexer(std::make_unique<Epoll::Multiplexer>())
            , _threadPool(maxPoolSize)
        {
            _socketInit(port);
            _UnifiedEventSource::bindSignal(SIGALRM);
        }

        /* Release connection resources */
        ~EventLoop()
        {
            socket.disconnect();
        }

        /* Set event handler */
        template<typename F, typename... Args>
        void setReadEventHandler(F&& handler, Args&&... args)
        {
            this->_readEvHandler = this->_handlerSetHelper(std::forward<F>(handler), std::forward<Args>(args)...);
        }

        template<typename F, typename... Args>
        void setWriteEventHandler(F&& handler, Args&&... args)
        {
            this->_writeEvHandler = this->_handlerSetHelper(std::forward<F>(handler), std::forward<Args>(args)...);
        }

        template<typename F, typename... Args>
        void setSignalEventHandler(int sig, F&& handler, Args&&... args)
        {
            _UnifiedEventSource::bindSignal(sig);
            auto handlerBinder = std::bind(std::forward<F>(handler), std::forward<Args>(args)...);
            _sigHandlerTbl[sig] = [handlerBinder]()->void
            {
                handlerBinder();
            };
        }

        template<typename F, typename... Args>
        void setTimeoutEventHandler(F&& handler, Args&&... args)
        {
            this->_timeoutCallback = this->_handlerSetHelper(std::forward<F>(handler), std::forward<Args>(args)...);
        }

        /* Do Event Loop */
        int run(std::uint16_t timeoutMs)
        {
            bool stop = false;
            _timer.startCount(timeoutMs);
            while(!stop)
            {
                _multiplexer->wait();
                for (auto cit = _multiplexer->begin(); cit != _multiplexer->end(); ++cit)
                {
                    _handleEvent(timeoutMs, *cit);
                }
            }
            return 0;
        }
	};
}
