#pragma once
#include "net_common.h"
#include "net_message.h"
#include "net_tsqueue.h"
#include "net_connection.h"

namespace cgull {
    namespace net {

        template <typename T>
        class client_interface
        {
        public:
            client_interface() : m_socket(m_context) {}
            ~client_interface() { Disconnect(); }

            bool Connect(const std::string& host, const uint16_t port)
            {
                try
                {
                    if (thrContext.joinable()) {
                        // If a previous run left the thread around, clean it up
                        m_context.stop();
                        thrContext.join();
                    }

                    if (m_context.stopped())
                        m_context.restart(); 

                    asio::ip::tcp::resolver resolver(m_context);
                    auto endpoints = resolver.resolve(host, std::to_string(port));

                    m_connection = std::make_unique<connection<T>>(
                        owner::client, m_context, asio::ip::tcp::socket(m_context), m_qMessagesIn);
                    m_connection->ConnectToServer(endpoints);

                    // Always ensure the context thread is running
                    thrContext = std::thread([this]() { m_context.run(); });
                }
                catch (std::exception& e)
                {
                    std::cerr << "[CLIENT] Exception: " << e.what() << "\n";
                    return false;
                }
                return true;
            }

            void Disconnect()
            {
                if (IsConnected())
                    m_connection->Disconnect();

                m_context.stop();
                if (thrContext.joinable()) thrContext.join();

                // Prepare for a clean future Connect()
                m_context.restart();
                m_connection.reset();
            }


            bool IsConnected() const
            {
                return m_connection ? m_connection->IsConnected() : false;
            }


            void Send(const message<T>& msg)
            {
                if (IsConnected())
                    m_connection->Send(msg);
            }

            tsqueue<owned_message<T>>& Incoming()
            {
                return m_qMessagesIn;
            }

        private:
            asio::io_context m_context;
            std::thread thrContext;

            asio::ip::tcp::socket m_socket;
            std::unique_ptr<connection<T>> m_connection;

        private:
            tsqueue<owned_message<T>> m_qMessagesIn;
        };

    }
} 
