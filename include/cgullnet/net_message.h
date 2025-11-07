#pragma once
#include "net_common.h"

namespace cgull
{
	namespace net
	{
		// message header is sent at the start of all messages, and by using a template
		// we can use a enum class to make sure message are valid at compile :)
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

			// returns size of message packet in bytes
			size_t size() const
			{
				return body.size();
			}

			// Overrides std::cout and makesa lil description of the message
			friend std::ostream& operator << (std::ostream& os, const message<T>& msg)
			{
				os << "ID:" << int(msg.header.id) << "Size:" << msg.header.size;
				return os;
			}

			// Pushes data into the message buffer
			template<typename DataType>
			friend message<T>& operator << (message<T>& msg, const DataType& data)
			{
				// Check that the type of the data being pushed is actually copyable (bad bad things happen if not)
				static_assert(std::is_standard_layout<DataType>::value, "Data is to complex to be pushed into vector");

				// Cache current size of the vector, because this will be where we insert the data
				size_t i = msg.body.size();

				// Resize the bector by the size of the data being pushed
				msg.body.resize(msg.body.size() + sizeof(DataType));

				// mem copy the data into the newly allocated vector space
				std::memcpy(msg.body.data() + i, &data, sizeof(DataType));

				msg.header.size = msg.size();

				// Return the target message so it can be "chained"
				return msg;

			}

			template<typename DataType>
			friend message<T>& operator >> (message<T>& msg, DataType& data)
			{ 
				// Check that the type of the data being pushed is agian copyable
				static_assert(std::is_standard_layout<DataType>::value, "Data is too complex to be pushed into vector");

				// Cache the location towards the end of the vector where the pulled data starts
				size_t i = msg.body.size() - sizeof(DataType);

				// Physically copy the data frm the vector inot the user variable
				std::memcpy(&data, msg.body.data() + i, sizeof(DataType));

				// Shrink the vector to remove read bytes and reset end position
				msg.body.resize(i);

				// Recalc message size
				msg.header.size = msg.size();

				// Return the target message so it can be "chained"
				return msg;
			}


		};

		// Forward declare the connection
		template <typename T>
		class connection; 


		template <typename T>
		struct owned_message
		{
			std::shared_ptr<connection<T>> remote = nullptr;
			message<T> msg;

			// Again a friendly string maker
			friend std::ostream& operator<<(std::ostream& os, const owned_message<T>& msg)
			{
				os << msg.msg;
				return os; 
			}
		};
	}
}