// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelStreaming2MediaTextureResource.h"

#include "DeviceProfiles/DeviceProfile.h"
#include "DeviceProfiles/DeviceProfileManager.h"

FPixelStreaming2MediaTextureResource::FPixelStreaming2MediaTextureResource(TWeakObjectPtr<UPixelStreaming2MediaTexture> Owner)
	: MediaTexture(Owner)
{
}

void FPixelStreaming2MediaTextureResource::InitRHI(FRHICommandListBase& RHICmdList)
{
	TStrongObjectPtr<UPixelStreaming2MediaTexture> PinnedMediaTexture = MediaTexture.Pin();
	if (PinnedMediaTexture.IsValid())
	{
		FSamplerStateInitializerRHI SamplerStateInitializer(
			(ESamplerFilter)UDeviceProfileManager::Get().GetActiveProfile()->GetTextureLODSettings()->GetSamplerFilter(PinnedMediaTexture.Get()),
			AM_Border, AM_Border, AM_Wrap);
		SamplerStateRHI = RHICreateSamplerState(SamplerStateInitializer);
	}
}

void FPixelStreaming2MediaTextureResource::ReleaseRHI()
{
	TextureRHI.SafeRelease();

	TStrongObjectPtr<UPixelStreaming2MediaTexture> PinnedMediaTexture = MediaTexture.Pin();
	if (PinnedMediaTexture.IsValid())
	{
		RHIUpdateTextureReference(PinnedMediaTexture->TextureReference.TextureReferenceRHI, nullptr);
	}
}

uint32 FPixelStreaming2MediaTextureResource::GetSizeX() const
{
	return TextureRHI.IsValid() ? TextureRHI->GetSizeXYZ().X : 0;
}

uint32 FPixelStreaming2MediaTextureResource::GetSizeY() const
{
	return TextureRHI.IsValid() ? TextureRHI->GetSizeXYZ().Y : 0;
}

SIZE_T FPixelStreaming2MediaTextureResource::GetResourceSize()
{
	return CalcTextureSize(GetSizeX(), GetSizeY(), EPixelFormat::PF_A8R8G8B8, 1);
}
