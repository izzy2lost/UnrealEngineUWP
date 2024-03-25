// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaNetworkBackend.h"
#include "UbaNetworkClient.h"

namespace uba
{
	struct NetworkMessage
	{
		NetworkMessage(NetworkClient& client, u8 serviceId, u8 messageType, BinaryWriter& sendWriter);
		~NetworkMessage();

		bool Send(BinaryReader& response);
		bool Send(BinaryReader& response, Timer& outTimer);
		bool Send();

		bool SendAsync(BinaryReader& response);
		bool WaitForAsync(BinaryReader& response);

	private:
		NetworkClient& m_client;
		BinaryWriter& m_sendWriter;
		Event m_event;
		void* m_response = nullptr;
		NetworkBackend::SendContext m_sendContext;
		u32 m_responseSize = 0;
		u32 m_responseCapacity = 0;
		u16 m_id = 0;
		Atomic<bool> m_error;
		NetworkClient::Connection* m_connection = nullptr;
		friend NetworkClient;
		NetworkMessage(const NetworkMessage&) = delete;
		NetworkMessage(NetworkMessage&&) = delete;
	};
}
