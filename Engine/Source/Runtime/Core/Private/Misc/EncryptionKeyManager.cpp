// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/EncryptionKeyManager.h"
#include "Async/UniqueLock.h"
#include "Misc/CoreDelegates.h"

namespace UE
{

FEncryptionKeyManager::FEncryptionKeyManager()
{
	if (FCoreDelegates::GetPakEncryptionKeyDelegate().IsBound())
	{
		FAES::FAESKey Key;
		FCoreDelegates::GetPakEncryptionKeyDelegate().Execute(Key.Key);

		if (Key.IsValid())
		{
			AddKey(FGuid(), Key);
		}
	}

	FCoreDelegates::GetRegisterEncryptionKeyMulticastDelegate().AddRaw(this, &FEncryptionKeyManager::AddKey);
}

FEncryptionKeyManager::~FEncryptionKeyManager()
{
	FCoreDelegates::GetRegisterEncryptionKeyMulticastDelegate().RemoveAll(this);
}

bool FEncryptionKeyManager::ContainsKey(const FGuid& Id)
{
	return nullptr != GetKey(Id);
}

void FEncryptionKeyManager::AddKey(const FGuid& Id, const FAES::FAESKey& Key)
{
	bool bAdded = false;
	{
		TUniqueLock Lock(Mutex);

		if (!Keys.Contains(Id))
		{
			Keys.Add(Id, Key);
			bAdded = true;
		}
	}

	if (bAdded && KeyAdded.IsBound())
	{
		KeyAdded.Broadcast(Id, Key);
	}
}

bool FEncryptionKeyManager::TryGetKey(const FGuid& Id, FAES::FAESKey& OutKey)
{
	if (FAES::FAESKey* Key = GetKey(Id))
	{
		OutKey = *Key;
		return true;
	}

	return false;
}

TMap<FGuid, FAES::FAESKey> FEncryptionKeyManager::GetAllKeys()
{
	TUniqueLock Lock(Mutex);
	return Keys;
}

FAES::FAESKey* FEncryptionKeyManager::GetKey(const FGuid& Id)
{
	TUniqueLock Lock(Mutex);
	return Keys.Find(Id);
}

FEncryptionKeyManager& FEncryptionKeyManager::Get()
{
	static FEncryptionKeyManager Mgr;
	return Mgr;
}

} // namespace UE
