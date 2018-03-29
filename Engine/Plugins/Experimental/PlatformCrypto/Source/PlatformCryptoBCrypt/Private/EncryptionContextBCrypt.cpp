// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "EncryptionContextBCrypt.h"
#include "PlatformCryptoBCryptModule.h"

// @ATG_CHANGE : BEGIN
// Removing the ECB mode entirely since it's insecure.  It provides confidentially but not authentication.
// Meaning, anyone between the two endpoints can choose to randomize data within the stream on a 16 byte (block size) basis.
// If someone combines this with packet analysis, an attacker can randomize targeted fields based on what is expected with a
// packet based on it’s size and others in the send sequence.  This could lead to crashes or even security bugs.
static const int32 AES_BlockSizeInBytes = 16;
static const int32 AES_GCM_NonceSizeInBytes = 12;
static const int32 AES_GCM_AuthenticatorSizeInBytes = 16;
static const int32 AES_GCM_PacketHeaderSizeInBytes = AES_GCM_NonceSizeInBytes + AES_GCM_AuthenticatorSizeInBytes;
// @ATG_CHANGE : END

class FScopedBCryptKey
{
public:
	// @ATG_CHANGE : BEGIN
	FScopedBCryptKey(BCRYPT_ALG_HANDLE ProviderHandle, const TArrayView<const uint8>& InKey)
		: KeyHandle(nullptr)
	// @ATG_CHANGE : END
	{
		// Get key object size
		DWORD KeyObjectSize = 0;
		ULONG BytesWritten = 0;
		const NTSTATUS GetKeySizeResult = BCryptGetProperty(ProviderHandle, BCRYPT_OBJECT_LENGTH, (PBYTE)&KeyObjectSize, sizeof(KeyObjectSize), &BytesWritten, 0);
		if (!BCRYPT_SUCCESS(GetKeySizeResult))
		{
			UE_LOG(LogPlatformCryptoBCrypt, Warning, TEXT("FScopedBCryptKey constructor: BCryptGetProperty failed to get key object size with code 0x%08x."), GetKeySizeResult);
			return;
		}

		KeyObject.SetNumUninitialized(KeyObjectSize);

		const NTSTATUS GenerateKeyResult = BCryptGenerateSymmetricKey(ProviderHandle, &KeyHandle, KeyObject.GetData(), KeyObject.Num(), const_cast<uint8*>(InKey.GetData()), InKey.Num(), 0);
		if (!BCRYPT_SUCCESS(GenerateKeyResult))
		{
			UE_LOG(LogPlatformCryptoBCrypt, Warning, TEXT("FScopedBCryptKey constructor: BCryptGenerateSymmetricKey failed to generate key object with code 0x%08x."), GenerateKeyResult);
			KeyObject.Reset();
			return;
		}
	}

	FScopedBCryptKey(const FScopedBCryptKey& Other) = delete;
	FScopedBCryptKey& operator=(const FScopedBCryptKey& Other) = delete;

	~FScopedBCryptKey()
	{
		BCryptDestroyKey(KeyHandle);
	}

	bool IsValid() const
	{
		return KeyHandle != nullptr && KeyObject.Num() != 0;
	}

	BCRYPT_KEY_HANDLE GetHandle() const
	{
		return KeyHandle;
	}

private:
	BCRYPT_KEY_HANDLE KeyHandle;
	TArray<uint8> KeyObject;
};

// @ATG_CHANGE : BEGIN - Switching ECB to GCM, and allowing other key sizes
TArray<uint8> FEncryptionContextBCrypt::Encrypt(const TArrayView<const uint8> Plaintext, const TArrayView<const uint8> Key, TArray<uint8>& NonceData, EPlatformCryptoResult& OutResult)
{
	return Encrypt_AES_GCM(Plaintext, Key, NonceData, OutResult);
}

TArray<uint8> FEncryptionContextBCrypt::Decrypt(const TArrayView<const uint8> Plaintext, const TArrayView<const uint8> Key, EPlatformCryptoResult& OutResult)
{
	return Decrypt_AES_GCM(Plaintext, Key, OutResult);
}

int32 FEncryptionContextBCrypt::GetMaxReservedBits()
{
	return (AES_GCM_PacketHeaderSizeInBytes + AES_BlockSizeInBytes) * 8;
}

TArray<uint8> FEncryptionContextBCrypt::Encrypt_AES_GCM(const TArrayView<const uint8> Plaintext, const TArrayView<const uint8> Key, TArray<uint8>& Nonce, EPlatformCryptoResult& OutResult)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("BCrypt AES Encrypt"), STAT_BCrypt_AES_Encrypt, STATGROUP_PlatformCrypto);

	OutResult = EPlatformCryptoResult::Failure;

	// Should be fine to hard code these, since this code path is already AES-GCM specific
	int KeyBits = Key.Num() * 8;
	if (KeyBits != 128 && KeyBits != 192 && KeyBits != 256)
	{
		UE_LOG(LogPlatformCryptoBCrypt, Warning, TEXT("FEncryptionContextBCrypt::Encrypt_AES_GCM: Key size %d is not a supported size (128/192/256)."), KeyBits);
		return TArray<uint8>();
	}

	BCRYPT_ALG_HANDLE ProviderHandle = FPlatformCryptoBCryptModule::Get().GetOrOpenProvider(BCRYPT_AES_ALGORITHM);

	if (ProviderHandle == nullptr)
	{
		UE_LOG(LogPlatformCryptoBCrypt, Warning, TEXT("FEncryptionContextBCrypt::Encrypt_AES_GCM: failed to open provider."));
		return TArray<uint8>();
	}

	BCRYPT_ALG_HANDLE RNGProviderHandle = FPlatformCryptoBCryptModule::Get().GetOrOpenProvider(BCRYPT_RNG_ALGORITHM);

	if (RNGProviderHandle == nullptr)
	{
		UE_LOG(LogPlatformCryptoBCrypt, Warning, TEXT("FEncryptionContextBCrypt::Encrypt_AES_GCM: failed to open RNG provider."));
		return TArray<uint8>();
	}

	const NTSTATUS SetChainResult = BCryptSetProperty(ProviderHandle, BCRYPT_CHAINING_MODE, (PBYTE)BCRYPT_CHAIN_MODE_GCM, sizeof(BCRYPT_CHAIN_MODE_GCM), 0);
	if (!BCRYPT_SUCCESS(SetChainResult))
	{
		UE_LOG(LogPlatformCryptoBCrypt, Warning, TEXT("FEncryptionContextBCrypt::Encrypt_AES_GCM: BCryptSetProperty failed to set chaining mode with code 0x%08x."), SetChainResult);
		return TArray<uint8>();
	}

	FScopedBCryptKey BCryptKey(ProviderHandle, Key);
	if (!BCryptKey.IsValid())
	{
		UE_LOG(LogPlatformCryptoBCrypt, Warning, TEXT("FEncryptionContextBCrypt::Encrypt_AES_GCM: failed to generate key object."));
		return TArray<uint8>();
	}

	// GCM streams get amended with a plaintext nonce, and ciphertext authenticator

	uint8 tagTemp[AES_GCM_AuthenticatorSizeInBytes];

	//struct NonceData
	//{
	//	uint32 Salt;	//Static defined by service, MUST BE UNIQUE BETWEEN CLIENT AND SERVER
	//	uint32 Rand;	//Randomized by BCryptGenRand every packet
	//	uint32 Counter; //Sequence Number
	//};

	uint32* NonceCounter = reinterpret_cast<uint32*>(Nonce.GetData() + 8);
	(*NonceCounter)++;

	const NTSTATUS RandomSeedResult = BCryptGenRandom(RNGProviderHandle, (Nonce.GetData() + 4), 4, 0);
	if (!BCRYPT_SUCCESS(SetChainResult))
	{
		UE_LOG(LogPlatformCryptoBCrypt, Warning, TEXT("FEncryptionContextBCrypt::Encrypt_AES_GCM: BCryptGenRandom failed to create random seed with code 0x%08x."), SetChainResult);
		return TArray<uint8>();
	}

	BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO AuthInfo;
	BCRYPT_INIT_AUTH_MODE_INFO(AuthInfo);
	AuthInfo.pbNonce = Nonce.GetData();// read the nonce from our temp buffer
	AuthInfo.cbNonce = AES_GCM_NonceSizeInBytes;
	AuthInfo.pbAuthData = nullptr;
	AuthInfo.cbAuthData = 0;
	AuthInfo.pbTag = tagTemp;  // generate the auth tag into the temporary buffer
	AuthInfo.cbTag = AES_GCM_AuthenticatorSizeInBytes;
	AuthInfo.pbMacContext = nullptr;
	AuthInfo.cbMacContext = 0;
	AuthInfo.cbAAD = 0;
	AuthInfo.cbData = 0;
	AuthInfo.dwFlags = 0;

	ULONG CiphertextSize = 0;
	const NTSTATUS GetEncryptSizeResult = BCryptEncrypt(BCryptKey.GetHandle(), const_cast<uint8*>(Plaintext.GetData()), Plaintext.Num(), &AuthInfo, nullptr, 0, nullptr, 0, &CiphertextSize, 0);
	if (!BCRYPT_SUCCESS(GetEncryptSizeResult))
	{
		UE_LOG(LogPlatformCryptoBCrypt, Warning, TEXT("FEncryptionContextBCrypt::Encrypt_AES_GCM: BCryptEncrypt failed to get ciphertext size with code 0x%08x."), GetEncryptSizeResult);
		return TArray<uint8>();
	}

	TArray<uint8> Ciphertext;
	Ciphertext.SetNumUninitialized(CiphertextSize + AES_GCM_PacketHeaderSizeInBytes);
	ULONG NumCiphertextBytesWritten = 0;

	const NTSTATUS EncryptResult = BCryptEncrypt(BCryptKey.GetHandle(), const_cast<uint8*>(Plaintext.GetData()), Plaintext.Num(), &AuthInfo, nullptr, 0, Ciphertext.GetData() + AES_GCM_PacketHeaderSizeInBytes, CiphertextSize, &NumCiphertextBytesWritten, 0);
	if (!BCRYPT_SUCCESS(EncryptResult))
	{
		UE_LOG(LogPlatformCryptoBCrypt, Warning, TEXT("FEncryptionContextBCrypt::Encrypt_AES_GCM: BCryptEncrypt failed to encrypt with code 0x%08x."), EncryptResult);
		return TArray<uint8>();
	}

	memcpy(Ciphertext.GetData(), Nonce.GetData(), AES_GCM_NonceSizeInBytes);
	memcpy(Ciphertext.GetData() + AES_GCM_NonceSizeInBytes, tagTemp, AES_GCM_AuthenticatorSizeInBytes);

	if ((NumCiphertextBytesWritten + AES_GCM_PacketHeaderSizeInBytes) != Ciphertext.Num())
	{
		UE_LOG(LogPlatformCryptoBCrypt, Warning, TEXT("FEncryptionContextBCrypt::Encrypt_AES_GCM: BCryptEncrypt didn't write correct number of bytes (%d) to ciphertext. Written: %d"), Ciphertext.Num(), NumCiphertextBytesWritten);
		return TArray<uint8>();
	}

	OutResult = EPlatformCryptoResult::Success;
	return Ciphertext;
}

TArray<uint8> FEncryptionContextBCrypt::Decrypt_AES_GCM(const TArrayView<const uint8> Ciphertext, const TArrayView<const uint8> Key, EPlatformCryptoResult& OutResult)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("BCrypt AES Decrypt"), STAT_BCrypt_AES_Decrypt, STATGROUP_PlatformCrypto);

	OutResult = EPlatformCryptoResult::Failure;

	// Should be fine to hard code these, since this code path is already AES-GCM specific
	int KeyBits = Key.Num() * 8;
	if (KeyBits != 128 && KeyBits != 192 && KeyBits != 256)
	{
		UE_LOG(LogPlatformCryptoBCrypt, Warning, TEXT("FEncryptionContextBCrypt::Decrypt_AES_GCM: Key size %d is not a supported size (128/192/256)."), KeyBits);
		return TArray<uint8>();
	}

	if (Ciphertext.Num() <= AES_GCM_PacketHeaderSizeInBytes)
	{
		//Malformed or empty packet
		UE_LOG(LogPlatformCryptoBCrypt, Warning, TEXT("FEncryptionContextBCrypt::Decrypt_AES_GCM: Packet recieved of too small size, discarding"));
		return TArray<uint8>();
	}

	BCRYPT_ALG_HANDLE ProviderHandle = FPlatformCryptoBCryptModule::Get().GetOrOpenProvider(BCRYPT_AES_ALGORITHM);

	if (ProviderHandle == nullptr)
	{
		UE_LOG(LogPlatformCryptoBCrypt, Warning, TEXT("FEncryptionContextBCrypt::Decrypt_AES_GCM: failed to open provider."));
		return TArray<uint8>();
	}

	const NTSTATUS SetChainResult = BCryptSetProperty(ProviderHandle, BCRYPT_CHAINING_MODE, (PBYTE)BCRYPT_CHAIN_MODE_GCM, sizeof(BCRYPT_CHAIN_MODE_GCM), 0);
	if (!BCRYPT_SUCCESS(SetChainResult))
	{
		UE_LOG(LogPlatformCryptoBCrypt, Warning, TEXT("FEncryptionContextBCrypt::Decrypt_AES_GCM: BCryptSetProperty failed to set chaining mode with code 0x%08x."), SetChainResult);
		return TArray<uint8>();
	}

	FScopedBCryptKey BCryptKey(ProviderHandle, Key);
	if (!BCryptKey.IsValid())
	{
		UE_LOG(LogPlatformCryptoBCrypt, Warning, TEXT("FEncryptionContextBCrypt::Decrypt_AES_GCM: failed to generate key object."));
		return TArray<uint8>();
	}

	BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO AuthInfo;
	BCRYPT_INIT_AUTH_MODE_INFO(AuthInfo);
	AuthInfo.pbNonce = PUCHAR(Ciphertext.GetData());  // Nonce lives at the start of the packet
	AuthInfo.cbNonce = AES_GCM_NonceSizeInBytes;
	AuthInfo.pbAuthData = nullptr;
	AuthInfo.cbAuthData = 0;
	AuthInfo.pbTag = PUCHAR(Ciphertext.GetData() + AES_GCM_NonceSizeInBytes); // Auth tag lives after nonce
	AuthInfo.cbTag = AES_GCM_AuthenticatorSizeInBytes;
	AuthInfo.pbMacContext = nullptr;
	AuthInfo.cbMacContext = 0;
	AuthInfo.cbAAD = 0;
	AuthInfo.cbData = 0;
	AuthInfo.dwFlags = 0;

	ULONG PlaintextSize = 0;
	const NTSTATUS GetDecryptSizeResult = BCryptDecrypt(BCryptKey.GetHandle(), const_cast<uint8*>(Ciphertext.GetData() + AES_GCM_PacketHeaderSizeInBytes), Ciphertext.Num() - AES_GCM_PacketHeaderSizeInBytes, &AuthInfo, nullptr, 0, nullptr, 0, &PlaintextSize, 0);
	if (!BCRYPT_SUCCESS(GetDecryptSizeResult))
	{
		UE_LOG(LogPlatformCryptoBCrypt, Warning, TEXT("FEncryptionContextBCrypt::Decrypt_AES_GCM: BCryptDecrypt failed to get plaintext size with code 0x%08x."), GetDecryptSizeResult);
		return TArray<uint8>();
	}

	TArray<uint8> Plaintext;
	Plaintext.SetNumUninitialized(PlaintextSize);
	ULONG NumPlaintextBytesWritten = 0;

	const NTSTATUS DecryptResult = BCryptDecrypt(BCryptKey.GetHandle(), const_cast<uint8*>(Ciphertext.GetData() + AES_GCM_PacketHeaderSizeInBytes), Ciphertext.Num() - AES_GCM_PacketHeaderSizeInBytes, &AuthInfo, nullptr, 0, Plaintext.GetData(), Plaintext.Num(), &NumPlaintextBytesWritten, 0);
	if (!BCRYPT_SUCCESS(DecryptResult))
	{
		UE_LOG(LogPlatformCryptoBCrypt, Warning, TEXT("FEncryptionContextBCrypt::Decrypt_AES_GCM: BCryptDecrypt failed to encrypt with code 0x%08x."), DecryptResult);
		return TArray<uint8>();
	}

	Plaintext.SetNum(NumPlaintextBytesWritten);

	OutResult = EPlatformCryptoResult::Success;
	return Plaintext;
}
// @ATG_CHANGE : END
