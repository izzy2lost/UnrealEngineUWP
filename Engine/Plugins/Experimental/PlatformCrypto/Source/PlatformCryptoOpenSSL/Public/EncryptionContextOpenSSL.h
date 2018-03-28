// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArrayView.h"
#include "PlatformCryptoTypes.h"

/**
 * Interface to certain cryptographic algorithms, using OpenSSL to implement them.
 */
class PLATFORMCRYPTOOPENSSL_API FEncryptionContextOpenSSL
{

public:
	// @ATG_CHANGE : BEGIN
	TArray<uint8> Encrypt(const TArrayView<const uint8> Plaintext, const TArrayView<const uint8> Key, TArray<uint8>& NonceData, EPlatformCryptoResult& OutResult)
	{
		return Encrypt_AES_256_ECB(Plaintext, Key, OutResult);
	}

	TArray<uint8> Decrypt(const TArrayView<const uint8> Ciphertext, const TArrayView<const uint8> Key, EPlatformCryptoResult& OutResult)
	{
		return Decrypt_AES_256_ECB(Ciphertext, Key, OutResult);
	}

	int32 GetMaxReservedBits() { return 16 * 8; }

private:
	// @ATG_CHANGE : END
	
	TArray<uint8> Encrypt_AES_256_ECB(const TArrayView<const uint8> Plaintext, const TArrayView<const uint8> Key, EPlatformCryptoResult& OutResult);

	TArray<uint8> Decrypt_AES_256_ECB(const TArrayView<const uint8> Ciphertext, const TArrayView<const uint8> Key, EPlatformCryptoResult& OutResult);
};

typedef FEncryptionContextOpenSSL FEncryptionContext;
