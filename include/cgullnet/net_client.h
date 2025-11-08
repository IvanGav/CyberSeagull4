#pragma once
#include "net_common.h"
#include "net_message.h"
#include "net_tsqueue.h"
#include "net_connection.h"
#include <mutex> 

namespace cgull {
    namespace net {

      

        template <typename T>
        class client_interface {
        public:
            client_interface() : m_socket(m_context) {}
            virtual ~client_interface() { Disconnect(); }

            bool Connect(const std::string& host, const uint16_t port) {
                std::lock_guard<std::mutex> lk(m_state_mtx);

                try {
                    if (thrContext.joinable()) { m_context.stop(); thrContext.join(); }
                    if (m_context.stopped()) { m_context.restart(); }

                    asio::ip::tcp::resolver resolver(m_context);
                    auto endpoints = resolver.resolve(host, std::to_string(port));

                    m_connection = std::make_unique<connection<T>>(
                        owner::client, m_context, asio::ip::tcp::socket(m_context), m_qMessagesIn);
                    m_connection->ConnectToServer(endpoints);

                    if (!thrContext.joinable())
                        thrContext = std::thread([this]() { m_context.run(); });
                }
                catch (const std::exception& e) {
                    std::cerr << "[CLIENT] Exception: " << e.what() << "\n";
                    return false;
                }
                return true;
            }

            void Disconnect() {
                std::lock_guard<std::mutex> lk(m_state_mtx);

                if (IsConnected()) m_connection->Disconnect();

                m_context.stop();
                if (thrContext.joinable()) thrContext.join();

                m_context.restart();
                m_connection.reset();
            }

            bool IsConnected() const {
                if (m_connection) return m_connection->IsConnected();
                return false;
            }

            void Send(const message<T>& msg) {
                if (IsConnected()) m_connection->Send(msg);
            }

        private:
            asio::io_context m_context;
            std::thread      thrContext;
            asio::ip::tcp::socket m_socket;
            std::unique_ptr<connection<T>> m_connection;
            tsqueue<owned_message<T>> m_qMessagesIn;

            mutable std::mutex m_state_mtx; 
        };


    }
} 
