// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Canvas.h"
#include <vector>

#include "DebugStorageServerConnection.generated.h"

class FStorageServerConnection;

UCLASS()
class UDebugStorageServerConnection : public UObject
{
	GENERATED_BODY()

public:
	void StartDrawing();
	void StopDrawing();

	void AddTimingInstance(double duration, uint64 bytes);

	void SetOwner(FStorageServerConnection* Connection)
	{
		Owner = Connection;
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
	uint64 AccumulatedBytes = 0;
	uint32 RequestCount = 0;

	double MinRequestThroughput = 0.0;
	double MaxRequestThroughput = 0.0;

	FCriticalSection StatsCS;
	FStorageServerConnection* Owner;

	static bool ShowGraphs;

};
