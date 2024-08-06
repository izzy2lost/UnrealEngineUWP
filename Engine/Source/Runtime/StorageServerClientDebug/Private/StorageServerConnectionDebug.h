// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Canvas.h"
#include <vector>
#include "IStorageServerPlatformFile.h"

#include "StorageServerConnectionDebug.generated.h"

UCLASS()
class UStorageServerConnectionDebug : public UObject
{
	GENERATED_BODY()

public:
	void StartDrawing();
	void StopDrawing();

	void SetPlatformFile(IStorageServerPlatformFile* InStorageServerPlatformFile)
	{
		StorageServerPlatformFile = InStorageServerPlatformFile;
		if (StorageServerPlatformFile != nullptr)
		{
			HostAddress = InStorageServerPlatformFile->GetHostAddr();
		}
		else
		{
			HostAddress.Reset();
		}
	}

	static void ShowGraph(FOutputDevice&);
	static void HideGraph(FOutputDevice&);

private:
	void Draw(UCanvas* Canvas, class APlayerController* PC);

	struct HistoryItem
	{
		double Time;
		double MaxRequestThroughput;
		double MinRequestThroughput;
		double Throughput;
		uint32 RequestCount;
	};

	std::vector<HistoryItem> History = {{0, 0, 0, 0, 0}};

	FDelegateHandle DrawHandle;

	static constexpr float UpdateStatsTimer = 1.0;
	double UpdateStatsTime = 0.0;

	IStorageServerPlatformFile* StorageServerPlatformFile = nullptr;
	FString HostAddress;

	static bool ShowGraphs;

};
