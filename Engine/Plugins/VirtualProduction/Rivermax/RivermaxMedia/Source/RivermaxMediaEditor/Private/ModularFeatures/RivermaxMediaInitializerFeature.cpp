// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModularFeatures/RivermaxMediaInitializerFeature.h"

#include "IRivermaxCoreModule.h"
#include "IRivermaxManager.h"

#include "RivermaxMediaSource.h"
#include "RivermaxMediaOutput.h"


bool FRivermaxMediaInitializerFeature::IsMediaSubjectSupported(const UObject* MediaSubject)
{
	if (MediaSubject)
	{
		return MediaSubject->IsA<URivermaxMediaSource>() || MediaSubject->IsA<URivermaxMediaOutput>();
	}

	return false;
}

void FRivermaxMediaInitializerFeature::InitializeMediaSubjectForTile(UObject* MediaSubject, const FString& OwnerName, uint8 OwnerUniqueIdx, const FIntPoint& TilePos)
{
	checkSlow(MediaSubject);

	if (URivermaxMediaSource* RivermaxMediaSource = Cast<URivermaxMediaSource>(MediaSubject))
	{
		RivermaxMediaSource->PlayerMode          = ERivermaxPlayerMode::Framelock;
		RivermaxMediaSource->bUseZeroLatency     = true;
		RivermaxMediaSource->bOverrideResolution = false;
		//RivermaxMediaSource->Resolution          = default value
		RivermaxMediaSource->FrameRate           = { 60,1 };
		RivermaxMediaSource->PixelFormat         = ERivermaxMediaSourcePixelFormat::RGB_10bit;
		RivermaxMediaSource->InterfaceAddress    = GetRivermaxInterfaceAddress();
		RivermaxMediaSource->StreamAddress       = GenerateStreamAddress(OwnerUniqueIdx, TilePos);
		RivermaxMediaSource->Port                = 50000;
		RivermaxMediaSource->bIsSRGBInput        = false;
		RivermaxMediaSource->bUseGPUDirect       = true;
	}
	else if (URivermaxMediaOutput* RivermaxMediaOutput = Cast<URivermaxMediaOutput>(MediaSubject))
	{
		RivermaxMediaOutput->AlignmentMode       = ERivermaxMediaAlignmentMode::FrameCreation;
		RivermaxMediaOutput->bDoContinuousOutput = false;
		RivermaxMediaOutput->FrameLockingMode    = ERivermaxFrameLockingMode::BlockOnReservation;
		RivermaxMediaOutput->PresentationQueueSize = 2;
		RivermaxMediaOutput->bDoFrameCounterTimestamping = true;
		RivermaxMediaOutput->bOverrideResolution = false;
		//RivermaxMediaOutput->Resolution          = default value
		RivermaxMediaOutput->FrameRate           = { 60,1 };
		RivermaxMediaOutput->PixelFormat         = ERivermaxMediaOutputPixelFormat::PF_10BIT_RGB;
		RivermaxMediaOutput->InterfaceAddress    = GetRivermaxInterfaceAddress();
		RivermaxMediaOutput->StreamAddress       = GenerateStreamAddress(OwnerUniqueIdx, TilePos);
		RivermaxMediaOutput->Port                = 50000;
		RivermaxMediaOutput->bUseGPUDirect       = true;
	}
}

FString FRivermaxMediaInitializerFeature::GetRivermaxInterfaceAddress() const
{
	FString ResultAddress{ TEXT("*.*.*.*") };

	// Now let's see if we have any interfaces available
	IRivermaxCoreModule& RivermaxModule = FModuleManager::LoadModuleChecked<IRivermaxCoreModule>(TEXT("RivermaxCore"));
	const TConstArrayView<UE::RivermaxCore::FRivermaxDeviceInfo> Devices = RivermaxModule.GetRivermaxManager()->GetDevices();
	if (Devices.Num() > 0)
	{
		// Split address into octets
		TArray<FString> Octets;
		Devices[0].InterfaceAddress.ParseIntoArray(Octets, TEXT("."));

		// IPv4?
		if (Octets.Num() == 4)
		{
			ResultAddress = FString::Printf(TEXT("%s.%s.%s.*"), *Octets[0], *Octets[1], *Octets[2]);
		}
		// IPv6?
		else
		{
			Devices[0].InterfaceAddress.ParseIntoArray(Octets, TEXT(":"));
			if (Octets.Num() == 6)
			{
				ResultAddress = FString::Printf(TEXT("%s:%s:%s:%s:%s:*"), *Octets[0], *Octets[1], *Octets[2], *Octets[3], *Octets[4]);
			}
		}
	}

	return ResultAddress;
}

FString FRivermaxMediaInitializerFeature::GenerateStreamAddress(uint8 OwnerUniqueIdx, const FIntPoint& TilePos) const
{
	constexpr uint8 MaxVal = TNumericLimits<uint8>::Max();
	checkSlow(OwnerUniqueIdx < MaxVal && TilePos.X < MaxVal && TilePos.Y < MaxVal);

	return FString::Printf(TEXT("228.%u.%u.%u"), OwnerUniqueIdx + 1, TilePos.X + 1, TilePos.Y + 1);
}
