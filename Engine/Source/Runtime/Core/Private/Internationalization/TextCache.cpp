// Copyright Epic Games, Inc. All Rights Reserved.

#include "Internationalization/TextCache.h"

#include "Containers/UnrealString.h"
#include "HAL/LowLevelMemTracker.h"
#include "Internationalization/TextKey.h"
#include "Misc/CString.h"
#include "Misc/LazySingleton.h"
#include "Misc/ScopeLock.h"
#include "AutoRTFM/AutoRTFM.h"

// Notes on the use of UE_AUTORTFM_ALWAYS_OPEN and UE_AUTORTFM_NOAUTORTFM:
// It is currently unsafe to use the cache in the open while an uncommitted
// transaction is in flight that has also touched the cache (#jira SOL-6743).
// RemoveCache() is not currently reachable from a closed transaction, so these
// are annotated with UE_AUTORTFM_NOAUTORTFM to prevent new transactional use.
// FindOrCache() is currently used from transactional code paths, so is
// annotated with UE_AUTORTFM_ALWAYS_OPEN.

FTextCache& FTextCache::Get()
{
	return TLazySingleton<FTextCache>::Get();
}

void FTextCache::TearDown()
{
	return TLazySingleton<FTextCache>::TearDown();
}

FText FTextCache::FindOrCache(const TCHAR* InTextLiteral, const TCHAR* InNamespace, const TCHAR* InKey)
{
	return FindOrCache(InTextLiteral, FTextId(InNamespace, InKey));
}

UE_AUTORTFM_ALWAYS_OPEN FText FTextCache::FindOrCache(const TCHAR* InTextLiteral, const FTextId& InTextId)
{
	LLM_SCOPE(ELLMTag::Localization);

	// First try and find a cached instance
	{
		FText* ReturnFoundText = nullptr;

		FScopeLock Lock(&CachedTextCS);

		FText* FoundText = CachedText.Find(InTextId);
		if (FoundText)
		{
			const FString* FoundTextLiteral = FTextInspector::GetSourceString(*FoundText);
			if (FoundTextLiteral && FCString::Strcmp(**FoundTextLiteral, InTextLiteral) == 0)
			{

				ReturnFoundText = FoundText;
			}
		}

		if (ReturnFoundText)
		{
			return *ReturnFoundText;
		}
	}

	// Not currently cached, make a new instance...
	FText NewText = FText(InTextLiteral, InTextId.GetNamespace(), InTextId.GetKey(), ETextFlag::Immutable);

	// ... and add it to the cache
	FScopeLock Lock(&CachedTextCS);

	CachedText.Emplace(InTextId, NewText);

	return NewText;
}

UE_AUTORTFM_NOAUTORTFM void FTextCache::RemoveCache(const FTextId& InTextId)
{
	return RemoveCache(MakeArrayView(&InTextId, 1));
}

UE_AUTORTFM_NOAUTORTFM void FTextCache::RemoveCache(TArrayView<const FTextId> InTextIds)
{
	FScopeLock Lock(&CachedTextCS);
	for (const FTextId& TextId : InTextIds)
	{
		CachedText.Remove(TextId);
	}
}

UE_AUTORTFM_NOAUTORTFM void FTextCache::RemoveCache(const TSet<FTextId>& InTextIds)
{
	FScopeLock Lock(&CachedTextCS);
	for (const FTextId& TextId : InTextIds)
	{
		CachedText.Remove(TextId);
	}
}
