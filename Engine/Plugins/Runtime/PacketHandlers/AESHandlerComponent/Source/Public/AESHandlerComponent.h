// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PacketHandler.h"
#include "IPlatformCrypto.h"
#include "EncryptionComponent.h"

/*
* AES256 ECB block encryption component.
*/
class AESHANDLERCOMPONENT_API FAESHandlerComponent : public FEncryptionComponent
{
public:
	/**
	 * Default constructor that leaves the Key empty, and encryption disabled.
	 * You must set the key before enabling encryption, or before receiving encrypted
	 * packets, or those operations will fail.
	 */
	FAESHandlerComponent();

	// @ATG_CHANGE : BEGIN
	// This handler uses AES, allow any key length by default
	static const int32 MinKeySizeInBytes = 16;

	// This handler uses AES GCM, which has 12-byte Nonce.
	static const int32 NonceSizeInBytes = 12;
	// @ATG_CHANGE : END


	// Replace the key used for encryption with NewKey if NewKey is exactly KeySizeInBytes long.
	virtual void SetEncryptionKey(TArrayView<const uint8> NewKey) override;

	// After calling this, future outgoing packets will be encrypted (until a call to DisableEncryption).
	virtual void EnableEncryption() override;

	// After calling this, future outgoing packets will not be encrypted (until a call to DisableEncryption).
	virtual void DisableEncryption() override;

	// @ATG_CHANGE : BEGIN 
	// Replace the NonceData used for encryption with NewData if NewData is exactly NonceSizeInBytes long.
	virtual void SetEncryptionNonceData(const TArrayView<const uint8>& NewData) override;
	// ATG_CHANGE : END

	// HandlerComponent interface
	virtual void Initialize() override;
	virtual bool IsValid() const override;
	virtual void Incoming(FBitReader& Packet) override;
	virtual void Outgoing(FBitWriter& Packet) override;
	virtual void IncomingConnectionless(FString Address, FBitReader& Packet) override;
	virtual void OutgoingConnectionless(FString Address, FBitWriter& Packet) override;
	virtual int32 GetReservedPacketBits() override;

private:
	TUniquePtr<FEncryptionContext> EncryptionContext;

	TArray<uint8> Key;
	// @ATG_CHANGE : BEGIN 
	TArray<uint8> NonceData;
	// ATG_CHANGE : END

	bool bEncryptionEnabled;
};


/**
 * The public interface to this module.
 */
class FAESHandlerComponentModule : public FPacketHandlerComponentModuleInterface
{
public:
	/* Creates an instance of this component */
	virtual TSharedPtr<HandlerComponent> CreateComponentInstance(FString& Options) override;
};
