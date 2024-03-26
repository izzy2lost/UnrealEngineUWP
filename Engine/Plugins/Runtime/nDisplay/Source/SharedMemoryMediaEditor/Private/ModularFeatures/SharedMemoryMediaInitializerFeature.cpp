// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModularFeatures/SharedMemoryMediaInitializerFeature.h"

#include "SharedMemoryMediaOutput.h"
#include "SharedMemoryMediaSource.h"


bool FSharedMemoryMediaInitializerFeature::IsMediaSubjectSupported(const UObject* MediaSubject)
{
	if (MediaSubject)
	{
		return MediaSubject->IsA<USharedMemoryMediaSource>() || MediaSubject->IsA<USharedMemoryMediaOutput>();
	}

	return false;
}

void FSharedMemoryMediaInitializerFeature::InitializeMediaSubjectForTile(UObject* MediaSubject, const FString& OwnerName, uint8 OwnerUniqueIdx, const FIntPoint& TilePos)
{
	checkSlow(MediaSubject);

	if (USharedMemoryMediaSource* SMMediaSource = Cast<USharedMemoryMediaSource>(MediaSubject))
	{
		SMMediaSource->bZeroLatency = true;
		SMMediaSource->Mode         = ESharedMemoryMediaSourceMode::Framelocked;
		SMMediaSource->UniqueName   = GenerateUniqueName(OwnerName, TilePos);
	}
	else if (USharedMemoryMediaOutput* SMMediaOutput = Cast<USharedMemoryMediaOutput>(MediaSubject))
	{
		SMMediaOutput->UniqueName   = GenerateUniqueName(OwnerName, TilePos);
		SMMediaOutput->bInvertAlpha = true;
		SMMediaOutput->bCrossGpu    = true;
		SMMediaOutput->NumberOfTextureBuffers = 4;
	}
}

FString FSharedMemoryMediaInitializerFeature::GenerateUniqueName(const FString& OwnerName, const FIntPoint& TilePos)
{
	return FString::Printf(TEXT("%s_tile_%d:%d"), *OwnerName, TilePos.X, TilePos.Y);
}
