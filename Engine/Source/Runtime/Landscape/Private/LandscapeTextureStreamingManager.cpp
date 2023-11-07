// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeTextureStreamingManager.h"
#include "Engine/Texture.h"

namespace UE::Landscape
{
	// double check that a texture is forced resident
	static inline void EnsureTextureForcedResident(UTexture *Texture)
	{
		// if other systems mess with this flag, then restore it to what it should be
		// Any code that is directly messing with the flag on one of our
		// landscape related textures should go through this streaming system instead
		if (!ensure(Texture->bForceMiplevelsToBeResident))
		{
			Texture->bForceMiplevelsToBeResident = true;
		}
	}
}

bool FLandscapeTextureStreamingManager::RequestTextureFullyStreamedIn(UTexture* Texture, bool bWaitForStreaming)
{
	TWeakObjectPtr<UTexture> TexturePtr = Texture;
	FTextureState& State = TextureStates.FindOrAdd(TexturePtr);

	if (State.RequestCount == 0)
	{
		Texture->bForceMiplevelsToBeResident = true;
	}
	else
	{
		UE::Landscape::EnsureTextureForcedResident(Texture);
	}
	State.RequestCount++;

	if (IsTextureFullyStreamedIn(Texture))
	{
		return true;
	}
	else if (bWaitForStreaming)
	{
		Texture->WaitForStreaming();
		return IsTextureFullyStreamedIn(Texture);
	}
	return false;
}

bool FLandscapeTextureStreamingManager::RequestTextureFullyStreamedInForever(UTexture* Texture, bool bWaitForStreaming)
{
	TWeakObjectPtr<UTexture> TexturePtr = Texture;
	FTextureState& State = TextureStates.FindOrAdd(TexturePtr);
	State.bForever = true;
	Texture->bForceMiplevelsToBeResident = true;

	if (IsTextureFullyStreamedIn(Texture))
	{
		return true;
	}
	else if (bWaitForStreaming)
	{
		Texture->WaitForStreaming();
		return IsTextureFullyStreamedIn(Texture);
	}
	return false;
}

void FLandscapeTextureStreamingManager::UnrequestTextureFullyStreamedIn(UTexture* Texture)
{
	if (Texture == nullptr)
	{
		return;
	}

	TWeakObjectPtr<UTexture> TexturePtr = Texture;
	FTextureState* State = TextureStates.Find(TexturePtr);
	if (State)
	{
		if (State->RequestCount > 0)
		{
			State->RequestCount--;
			if (!State->bForever && State->RequestCount <= 0)
			{
				// allow stream out, remove tracking
				Texture->bForceMiplevelsToBeResident = false;
				TextureStates.Remove(TexturePtr);
			}
			else
			{
				UE::Landscape::EnsureTextureForcedResident(Texture);
			}
		}
		else
		{
			// only way the request count should get to zero is if the texture is flagged as forever streamed.
			ensure(State->bForever);
			UE::Landscape::EnsureTextureForcedResident(Texture);
		}
	}
}

bool FLandscapeTextureStreamingManager::WaitForTextureStreaming()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(LandscapeTextureStreamingaAnager_WaitForTextureStreaming);
	bool bFullyStreamed = true;
	for (auto It = TextureStates.CreateIterator(); It; ++It)
	{
		UTexture* Texture = It.Key().Get();
		if (Texture)
		{
			UE::Landscape::EnsureTextureForcedResident(Texture);
			if (!Texture->IsFullyStreamedIn())
			{
				Texture->WaitForStreaming();
			}
#if WITH_EDITOR
			bFullyStreamed = bFullyStreamed && !Texture->IsDefaultTexture();
#endif // WITH_EDITOR		
			bFullyStreamed = bFullyStreamed && Texture->IsFullyStreamedIn();
		}
		else
		{
			// the texture was unloaded... we can remove this entry
			It.RemoveCurrent();
		}
	}
	return bFullyStreamed;
}

void FLandscapeTextureStreamingManager::CleanupInvalidEntries()
{
	for (auto It = TextureStates.CreateIterator(); It; ++It)
	{
		TWeakObjectPtr<UTexture>& TexPtr = It.Key();
		if (!TexPtr.IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

bool FLandscapeTextureStreamingManager::IsTextureFullyStreamedIn(UTexture* InTexture)
{
	return InTexture &&
#if WITH_EDITOR
		!InTexture->IsDefaultTexture() &&
#endif // WITH_EDITOR
		!InTexture->HasPendingInitOrStreaming() && InTexture->IsFullyStreamedIn();
}

FLandscapeTextureStreamingManager::~FLandscapeTextureStreamingManager()
{
	if (!ensure(TextureStates.IsEmpty()))
	{
		// clear force stream flag on all textures, just in case
		for (auto It = TextureStates.CreateIterator(); It; ++It)
		{
			UTexture* Texture = It.Key().Get();
			if (Texture)
			{
				Texture->bForceMiplevelsToBeResident = false;
			}
		}
	}
}