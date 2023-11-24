// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Compute/ComputeChannel.h"
#include "Compute/ComputeBuffer.h"
#include "Compute/ComputeSocket.h"
#include "Compute/ComputePlatform.h"
#include <iostream>
#include <thread>
#include <assert.h>

extern TCHAR GInternalProjectName[64] = { 0, };
extern const TCHAR* GForeignEngineDir = nullptr;

int main(int argc, const char* argv[])
{
	if (argc >= 2 && !FCStringAnsi::Stricmp(argv[1], "-Test"))
	{
		void RunTests();
		RunTests();
		return 0;
	}

	const int ChannelId = 100;

	FWorkerComputeSocket Socket;
	if (!Socket.Open())
	{
		std::cout << "Environment variable not set correctly" << std::endl;
		return 1;
	}

	TSharedPtr<FComputeChannel> Channel = Socket.CreateChannel(ChannelId);
	if(!Channel->IsValid())
	{
		std::cout << "Unable to create channel to initiator" << std::endl;
		return 1;
	}

	std::cout << "Connected to initiator" << std::endl;

	size_t Length = 0;
	char Buffer[4];

	Buffer[0] = 0;
	Channel->Send(Buffer, 1); // Send a dummy one-byte message to let the remote know we're listening

	for (;;)
	{
		size_t RecvLength = Channel->Recv(Buffer + Length, sizeof(Buffer) - Length);
		if (RecvLength == 0)
		{
			return 0;
		}

		Length += RecvLength;

		if (Length >= 4)
		{
			std::cout << "Read value " << *(int*)Buffer << std::endl;
			Length = 0;
		}
	}
}
