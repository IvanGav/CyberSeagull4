#pragma once
#include "net_common.h"
#include "net_tsqueue.h"
#include "net_message.h"

namespace cgull {
    namespace net {

        enum class owner { server, client };

        template<typename T>
        class connection : public std::enable_shared_from_this<connection<T>>
        {
        public:
            connection(owner parent, asio::io_context& asioContext, asio_tcp::socket socket, tsqueue<owned_message<T>>& qIn)
                : m_asioContext(asioContext), m_socket(std::move(socket)), m_qMessagesIn(qIn)
            {
                m_nOwnerType = parent;
            }

            virtual ~connection() {}

            uint32_t GetID() const { return id; }

            void ConnectToClient(uint32_t uid = 0)
            {
                if (m_nOwnerType == owner::server)
                {
                    if (m_socket.is_open())
                    {
                        id = uid;
                        m_connected = true;
                        ReadHeader();
                    }
                }
            }

            void ConnectToServer(const asio_tcp::resolver::results_type& endpoints)
            {
                if (m_nOwnerType == owner::client)
                {
                    asio::async_connect(m_socket, endpoints,
                        [this](std::error_code ec, asio_tcp::endpoint endpoint)
                        {
                            if (!ec)
                            {
                                m_connected = true;
                                ReadHeader();
                            }
                            else
                            {
                                m_connected = false;
                            }
                        });
                }
            }

            void Disconnect()
            {
                if (IsConnected())
                    asio::post(m_asioContext, [this]() { m_connected = false;  m_socket.close(); });
            }

            bool IsConnected() const
            {
                return m_connected.load() && m_socket.is_open();
            }


            void StartListening()
            {
                
            }

            // ASYNC - Send a message, add to the outgoing queue
            void Send(const message<T>& msg)
            {
                asio::post(m_asioContext,
                    [this, msg]()
                    {
                        bool bWritingMessage = !m_qMessagesOut.empty();
                        m_qMessagesOut.push_back(msg);
                        if (!bWritingMessage)
                        {
                            WriteHeader();
                        }
                    });
            }

        private:
            // ASYNC - Read a message header
            void ReadHeader()
            {
                asio::async_read(m_socket, asio::buffer(&m_msgTemporaryIn.header, sizeof(message_header<T>)),
                    [this](std::error_code ec, std::size_t /*length*/)
                    {
                        if (!ec)
                        {
                            if (m_msgTemporaryIn.header.size > 0)
                            {
                                m_msgTemporaryIn.body.resize(m_msgTemporaryIn.header.size);
                                ReadBody();
                            }
                            else
                            {
                                AddToIncomingMessageQueue();
                            }
                        }
                        else
                        {
                            // connection dead
                            std::cout << "[CONN] ReadHeader error: " << ec.message() << "\n";
                            m_connected = false; m_socket.close();
                        }
                    });
            }

            // ASYNC - Read a message body
            void ReadBody()
            {
                asio::async_read(m_socket, asio::buffer(m_msgTemporaryIn.body.data(), m_msgTemporaryIn.body.size()),
                    [this](std::error_code ec, std::size_t /*length*/)
                    {
                        if (!ec)
                        {
                            AddToIncomingMessageQueue();
                        }
                        else
                        {
                            std::cout << "[CONN] ReadBody error: " << ec.message() << "\n";
                            m_connected = false; m_socket.close();
                        }
                    });
            }

            // Pushes incoming message to queue
            void AddToIncomingMessageQueue()
            {
                if (m_nOwnerType == owner::server)
                    m_qMessagesIn.push_back({ this->shared_from_this(), m_msgTemporaryIn });
                else
                    m_qMessagesIn.push_back({ nullptr, m_msgTemporaryIn });

                ReadHeader();
            }

            // ASYNC - Write a message header
            void WriteHeader()
            {
                asio::async_write(m_socket, asio::buffer(&m_qMessagesOut.front().header, sizeof(message_header<T>)),
                    [this](std::error_code ec, std::size_t /*length*/)
                    {
                        if (!ec)
                        {
                            if (m_qMessagesOut.front().body.size() > 0)
                            {
                                WriteBody();
                            }
                            else
                            {
                                m_qMessagesOut.pop_front();
                                if (!m_qMessagesOut.empty())
                                {
                                    WriteHeader();
                                }
                            }
                        }
                        else
                        {
                            std::cout << "[CONN] WriteHeader error: " << ec.message() << "\n";
                            m_connected = false; m_socket.close();
                        }
                    });
            }

            // ASYNC - Write the message body
            void WriteBody()
            {
                asio::async_write(m_socket, asio::buffer(m_qMessagesOut.front().body.data(), m_qMessagesOut.front().body.size()),
                    [this](std::error_code ec, std::size_t /*length*/)
                    {
                        if (!ec)
                        {
                            m_qMessagesOut.pop_front();
                            if (!m_qMessagesOut.empty())
                            {
                                WriteHeader();
                            }
                        }
                        else
                        {
                            std::cout << "[CONN] WriteBody error: " << ec.message() << "\n";
                            m_connected = false; m_socket.close();
                        }
                    });
            }

        private:
            asio::io_context& m_asioContext;
            asio_tcp::socket m_socket;
            tsqueue<message<T>> m_qMessagesOut;
            tsqueue<owned_message<T>>& m_qMessagesIn;
            message<T> m_msgTemporaryIn;
            std::atomic<bool> m_connected{ false };
            owner m_nOwnerType = owner::server;
            uint32_t id = 0;
        };

    }
} 
