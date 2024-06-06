// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaCrypto.h"
#include "UbaNetwork.h"

namespace uba
{
	bool TestCrypto(Logger& logger, const StringBufferBase& rootDir)
	{
		u64 key128[] = { 0x1234567812345678llu, 0x1234567812345678llu };

		CryptoKey key = Crypto::CreateKey(logger, (const u8*)key128);

		u8 encryptedData[sizeof(EncryptionHandshakeString)];
		memcpy(encryptedData, EncryptionHandshakeString, sizeof(EncryptionHandshakeString));

		for (u32 i=0; i!=3; ++i)
		{
			if (!Crypto::Encrypt(logger, key, encryptedData, sizeof(encryptedData)))
				return false;

			if (memcmp(encryptedData, EncryptionHandshakeString, sizeof(EncryptionHandshakeString)) == 0)
				return false;

			if (!Crypto::Decrypt(logger, key, encryptedData, sizeof(encryptedData)))
				return false;

			if (memcmp(encryptedData, EncryptionHandshakeString, sizeof(EncryptionHandshakeString)) != 0)
				return false;

			if (i == 1)
			{
				CryptoKey newKey = Crypto::DuplicateKey(logger, key);
				Crypto::DestroyKey(key);
				key = newKey;
			}
		}

		Crypto::DestroyKey(key);
		return true;
	}
}