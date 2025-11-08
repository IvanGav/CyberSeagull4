#pragma once
#include "net_common.h"

namespace cgull {
    namespace net {

        template <typename T>
        struct message_header
        {
            T id{};
            uint32_t size = 0;
        };

        template <typename T>
        struct message
        {
            message_header<T> header{};
            std::vector<uint8_t> body;

            size_t size() const
            {
                return body.size();
            }

            template<typename DataType>
            friend message<T>& operator<<(message<T>& msg, const DataType& data)
            {
                static_assert(std::is_trivially_copyable<DataType>::value, "Data is too complex to be pushed into message.");

                const size_t old_size = msg.body.size();
                msg.body.resize(old_size + sizeof(DataType));
                std::memcpy(msg.body.data() + old_size, &data, sizeof(DataType));
                msg.header.size = static_cast<uint32_t>(msg.body.size());
                return msg;
            }

            template<typename DataType>
            friend message<T>& operator>>(message<T>& msg, DataType& data)
            {
                static_assert(std::is_trivially_copyable<DataType>::value, "Data is too complex to be pulled from message.");
                const size_t need = sizeof(DataType);
                const size_t have = msg.body.size();
                if (have < need)
                {
                    // You can assert in debug, but throwing is safest in release
                    throw std::runtime_error("message underflow: not enough bytes to extract requested DataType");
                }

                const size_t i = have - need; // safe now
                std::memcpy(&data, msg.body.data() + i, need);
                msg.body.resize(i);

                // header.size tracks body bytes only
                msg.header.size = static_cast<uint32_t>(msg.body.size());
                return msg;
            }

            friend std::ostream& operator<<(std::ostream& os, const message<T>& msg)
            {
                os << "ID:" << int(msg.header.id) << " Size:" << msg.header.size;
                return os;
            }
        };

        template <typename T>
        class connection;

        template <typename T>
        struct owned_message
        {
            std::shared_ptr<connection<T>> remote = nullptr;
            message<T> msg;

            friend std::ostream& operator<<(std::ostream& os, const owned_message<T>& owned)
            {
                os << owned.msg;
                return os;
            }
        };

    }
} 
