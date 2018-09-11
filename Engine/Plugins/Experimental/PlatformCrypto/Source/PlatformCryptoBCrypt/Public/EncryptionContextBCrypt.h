// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "PlatformCryptoTypes.h"

/**
 * Interface to certain cryptographic algorithms, using BCrypt/CNG to implement them.
 */
class PLATFORMCRYPTOBCRYPT_API FEncryptionContextBCrypt
{
public:
	// @ATG_CHANGE : BEGIN
	TArray<uint8> Encrypt(const TArrayView<const uint8> Plaintext, const TArrayView<const uint8> Key, TArray<uint8>& NonceData, EPlatformCryptoResult& OutResult);

	TArray<uint8> Decrypt(const TArrayView<const uint8> Ciphertext, const TArrayView<const uint8> Key, EPlatformCryptoResult& OutResult);

	int32 GetMaxReservedBits();

private:
	TArray<uint8> Encrypt_AES_GCM(const TArrayView<const uint8> Plaintext, const TArrayView<const uint8> Key, TArray<uint8>& Nonce, EPlatformCryptoResult& OutResult);

	TArray<uint8> Decrypt_AES_GCM(const TArrayView<const uint8> Ciphertext, const TArrayView<const uint8> Key, EPlatformCryptoResult& OutResult);
	// @ATG_CHANGE : END
};

typedef FEncryptionContextBCrypt FEncryptionContext;
