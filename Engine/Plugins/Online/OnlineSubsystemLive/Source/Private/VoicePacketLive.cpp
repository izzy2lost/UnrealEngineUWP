// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemLivePrivatePCH.h"
#include "VoicePacketLive.h"
#include "OnlineSubsystemLiveTypes.h"
#include "Online.h"
#include "Interfaces/OnlineIdentityInterface.h"

/**
 * Copies another packet
 *
 * 
 */
FVoicePacketLive::FVoicePacketLive(const FVoicePacketLive& Other) :
	FVoicePacket(Other)
{
	Reliable = Other.Reliable;
	Broadcast = Other.Broadcast;
	Sender = Other.Sender;
	Target = Other.Target;
	Length = Other.Length;
	PacketIndex = Other.PacketIndex;

	// Copy the contents of the voice packet
	Buffer.Empty(Other.Length);
	Buffer.AddUninitialized(Other.Length);
	FMemory::Memcpy(Buffer.GetData(), Other.Buffer.GetData(), Other.Length);
}

/** Returns the amount of space this packet will consume in a buffer */
uint16 FVoicePacketLive::GetTotalPacketSize()
{
	return sizeof(Reliable) + sizeof(Broadcast) + Target->GetSize() + Sender->GetSize() + sizeof(Length) + Length + sizeof(PacketIndex);
}

/** @return the amount of space used by the internal voice buffer */
uint16 FVoicePacketLive::GetBufferSize()
{
	return Length;
}

/** @return the sender of this voice packet */
TSharedPtr<const FUniqueNetId> FVoicePacketLive::GetSender()
{
	return Sender;
}

/** 
 * Serialize the voice packet data to a buffer 
 *
 * @param Ar buffer to write into
 */
void FVoicePacketLive::Serialize(class FArchive& Ar)
{
	// Make sure not to overflow the buffer by reading an invalid amount
	if (Ar.IsLoading())
	{
		Ar << Reliable;
		Ar << Broadcast;
		FString SenderUID;
		Ar << SenderUID;
		Sender = MakeShared<FUniqueNetIdLive>(SenderUID);
		FString TargetUID;
		Ar << TargetUID;
		Target = MakeShared<FUniqueNetIdLive>(TargetUID);
		Ar << Length;
		// Verify the packet is a valid size
		if (Length <= MAX_VOICE_DATA_SIZE)
		{
			Buffer.Empty(Length);
			Buffer.AddUninitialized(Length);
			Ar.Serialize(Buffer.GetData(), Length);
		}
		else
		{
			check(false);
			Length = 0;
		}
		Ar << PacketIndex;
	}
	else
	{
		Ar << Reliable;
		Ar << Broadcast;
		FString SenderString = Sender->ToString();
		Ar << SenderString;

		FString TargetString = Target->ToString();
		Ar << TargetString;
		Ar << Length;

		// Always safe to save the data as the voice code prevents overwrites
		Ar.Serialize(Buffer.GetData(), Length);

		Ar << PacketIndex;
	}
}



