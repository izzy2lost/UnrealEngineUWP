// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Net/VoiceDataCommon.h"
//#include "OnlineSubsystemLivePackage.h"

class FUniqueNetId;

/** Defines the data involved in a Live voice packet */
class FVoicePacketLive : public FVoicePacket
{

PACKAGE_SCOPE:

	/** The unique net id of the talker sending the data */
	TSharedPtr<const FUniqueNetId> Sender;
	/** The unique net id of the machine we're sending the data to, empty if a broadcast message */
	TSharedPtr<const FUniqueNetId> Target;
	/** The data that is to be sent/processed */
	TArray<uint8> Buffer;
	/** The current amount of space used in the buffer for this packet */
	uint16 Length;

	uint32	PacketIndex;

	bool Broadcast;
	bool Reliable;

public:
	/** Zeros members and validates the assumptions */
	FVoicePacketLive() :
		Broadcast(false),
		Reliable(false),
		Target(NULL),
		Sender(NULL),
		Length(0),
		PacketIndex(0)
	{
		Buffer.Empty(MAX_VOICE_DATA_SIZE);
	}

	/** Should only be used by TSharedPtr and FVoiceData */
	virtual ~FVoicePacketLive()
	{
	}

	/**
	 * Copies another packet and inits the ref count
	 *
	 * @param Other packet to copy
	 * @param InRefCount the starting ref count to use
	 */
	FVoicePacketLive(const FVoicePacketLive& Other);

	/** Returns the amount of space this packet will consume in a buffer */
	virtual uint16 GetTotalPacketSize() override;

	/** @return the amount of space used by the internal voice buffer */
	virtual uint16 GetBufferSize() override;

	/** @return the sender of this voice packet */
	virtual TSharedPtr<const FUniqueNetId> GetSender() override;

	virtual bool IsReliable() override { return Reliable; }

	/** 
	 * Serialize the voice packet data to a buffer 
	 *
	 * @param Ar buffer to write into
	 */
	virtual void Serialize(class FArchive& Ar) override;
};

