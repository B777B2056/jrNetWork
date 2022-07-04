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

namespace jrNetWork
{
    namespace _UnifiedEventSource
    {
        static int _uesfd[2];

        // Init ues
        static void init()
        {
            if (-1 == ::socketpair(AF_UNIX, SOCK_STREAM, 0, _uesfd))
            {
                LOGFATAL() << "UES endpoint init failed: " << ::strerror(errno) << std::endl;
            }
            // Set ues fd write non-blocking
            ::fcntl(_uesfd[1], F_SETFL, ::fcntl(_uesfd[1], F_GETFL) | O_NONBLOCK);
        }

        // Close fd
        static void closeUes()
        {
            ::close(_uesfd[1]);
            ::close(_uesfd[0]);
        }

        // Wrire signal to uesfd
        static void signalTransfer(int sig)
        {
            LOGNOTICE() << "Catch signal, signal " << sig << std::endl;
            ::write(_uesfd[1], reinterpret_cast<char*>(&sig), 1);
        }
        
        // Bind signal
        static void bindSignal(int sig)
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

        static bool handleSignals(std::unordered_map<int, std::function<void()> >& sigHandlerTbl)
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
        /* Event handlers */
        IOCallbackType _readEvHandler;
        IOCallbackType _writeEvHandler;
        TimeoutCallbackType _timeoutCallback;
        /* Timer */
        TimerContainer<SocketType> _timer;
        /* Thread pool */
        ThreadPool _threadPool;
        /* Event List */
        std::deque<Event> _activateEvents;
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
            _UnifiedEventSource::init();
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
                        if (!_idSocketTbl[ev.id]->is_send_all())
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
            if (!cltPtr->is_send_all())
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
                handlerBinder(cltPtr);
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
            _UnifiedEventSource::closeUes();
            socket.disconnect();
        }

        /* Set event handler */
        template<typename F, typename... Args>
        void setReadEventHandler(F&& handler, Args&&... args)
        {
            _readEvHandler = _handlerSetHelper(std::forward<F>(handler), std::forward<Args>(args)...);
        }

        template<typename F, typename... Args>
        void setWriteEventHandler(F&& handler, Args&&... args)
        {
            _writeEvHandler = _handlerSetHelper(std::forward<F>(handler), std::forward<Args>(args)...);
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
            _timeoutCallback = _handlerSetHelper(std::forward<F>(handler), std::forward<Args>(args)...);
        }

        /* Do Event Loop */
        int run(std::uint16_t timeoutMs)
        {
            bool stop = false;
            _timer.startCount(timeoutMs);
            while(!stop)
            {
                _activateEvents.clear();
                _multiplexer->wait(_activateEvents);
                for (Event& ev : _activateEvents)
                {
                    _handleEvent(timeoutMs, ev);
                }
            }
            return 0;
        }
	};
}
