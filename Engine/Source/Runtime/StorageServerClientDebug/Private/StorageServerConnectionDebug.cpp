// Copyright Epic Games, Inc. All Rights Reserved.

#include "StorageServerConnectionDebug.h"

#include "Debug/DebugDrawService.h"
#include "Engine/GameEngine.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "Templates/UniquePtr.h"
#include "StorageServerClientModule.h"

CSV_DEFINE_CATEGORY(ZenServerStats, true);

CSV_DEFINE_STAT(ZenServerStats, ThroughputMbps);
CSV_DEFINE_STAT(ZenServerStats, MaxReqThroughputMbps);
CSV_DEFINE_STAT(ZenServerStats, MinReqThroughputMbps);
CSV_DEFINE_STAT(ZenServerStats, RequestCountPerSec);

bool UStorageServerConnectionDebug::ShowGraphs = false;

void UStorageServerConnectionDebug::StartDrawing()
{
	if (DrawHandle.IsValid())
	{
		return;
	}
	DrawHandle = UDebugDrawService::Register(TEXT("Game"),FDebugDrawDelegate::CreateUObject(this, &UStorageServerConnectionDebug::Draw));
}

void UStorageServerConnectionDebug::StopDrawing()
{
	if (!DrawHandle.IsValid())
	{
		return;
	}

	UDebugDrawService::Unregister(DrawHandle);
	DrawHandle.Reset();
}

void UStorageServerConnectionDebug::Draw(UCanvas* Canvas, APlayerController*)
{
	static constexpr double FrameSeconds = 1.0;
	static constexpr float  ViewXRel = 0.2f;
	static constexpr float  ViewYRel = 0.12f;
	static constexpr float  ViewWidthRel = 0.4f;
	static constexpr float  ViewHeightRel = 0.18f;
	static constexpr double TextHeight = 16.0;
	static constexpr int    OneMinuteSeconds = 60;
	static constexpr double WidthSeconds = OneMinuteSeconds * 0.25;
	static constexpr double MaxHeightScaleThroughput = 6000;
	static constexpr double MaxHeightScaleRequest = 5000;
	static constexpr int	LineThickness = 3;
	static double			HeightScaleThroughput = MaxHeightScaleThroughput;
	static double			HeightScaleRequest = MaxHeightScaleRequest;
	
	double StatsTimeNow = FPlatformTime::Seconds();
	double Duration = StatsTimeNow - UpdateStatsTime;

	static double MaxReqThroughput = 0.0;
	static double MinReqThroughput = 0.0;
	static uint32 ReqCount = 0;
	static double Throughput = 0.0;

	//Persistent debug message and CSV stats
	if ((Duration > UpdateStatsTimer) && (GEngine) && StorageServerPlatformFile)
	{
		UpdateStatsTime = StatsTimeNow;

		IStorageServerPlatformFile::FConnectionStats Stats;
		StorageServerPlatformFile->GetAndResetConnectionStats(Stats);
		if (Stats.MaxRequestThroughput > Stats.MinRequestThroughput)
		{
			MaxReqThroughput = Stats.MaxRequestThroughput;
			MinReqThroughput = Stats.MinRequestThroughput;
		
			Throughput = ((double)(Stats.AccumulatedBytes * 8) / Duration) / 1000000.0; //Mbps
			ReqCount = ceil((double)Stats.RequestCount / Duration);
		}

		FString ZenConnectionDebugMsg;
		ZenConnectionDebugMsg = FString::Printf(TEXT("ZenServer streaming from %s [%.2fMbps]"), *HostAddress, Throughput);
		GEngine->AddOnScreenDebugMessage((uint64)this, 86400.0f, FColor::White, ZenConnectionDebugMsg, false);
		
		History.push_back({ StatsTimeNow, MaxReqThroughput, MinReqThroughput, Throughput, ReqCount });
	}

	while (!History.empty() && StatsTimeNow - History.front().Time > WidthSeconds)
	{
		History.erase(History.begin());
	}

	//CSV stats need to be written per frame
	CSV_CUSTOM_STAT_DEFINED(ThroughputMbps, Throughput, ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT_DEFINED(MaxReqThroughputMbps, MaxReqThroughput, ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT_DEFINED(MinReqThroughputMbps, MinReqThroughput, ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT_DEFINED(RequestCountPerSec, (int32)ReqCount, ECsvCustomStatOp::Set);

	if (ShowGraphs)
	{
		int ViewX = (int)(ViewXRel * Canvas->ClipX);
		int ViewY = (int)(ViewYRel * Canvas->ClipY);
		int ViewWidth = (int)(ViewWidthRel * Canvas->ClipX);;
		int ViewHeight = (int)(ViewHeightRel * Canvas->ClipY);;
		double PixelsPerSecond = ViewWidth / WidthSeconds;

		auto DrawLine =
			[Canvas](double X0, double Y0, double X1, double Y1, const FLinearColor& Color, double Thickness)
			{
				FCanvasLineItem Line{ FVector2D{X0, Y0}, FVector2D{X1, Y1} };
				Line.SetColor(Color);
				Line.LineThickness = Thickness;
				Canvas->DrawItem(Line);
			};

		auto DrawString =
			[Canvas](const FString& Str, int X, int Y, bool bCentre = true)
			{
				FCanvasTextItem Text{ FVector2D(X, Y), FText::FromString(Str), GEngine->GetTinyFont(), FLinearColor::Yellow };
				Text.EnableShadow(FLinearColor::Black);
				Text.bCentreX = bCentre;
				Text.bCentreY = bCentre;
				Canvas->DrawItem(Text);
			};

		double MaxValueInHistory = 0.0;

		if (History.size())
		{
			ViewY += TextHeight;
			DrawString(FString::Printf(TEXT("Request Throughput MIN/MAX: [%.2f] / [%.2f] Mbps"), History[History.size()-1].MinRequestThroughput, History[History.size()-1].MaxRequestThroughput), ViewX, ViewY, false);
			ViewY += TextHeight;
		}

		//FIRST GRAPH
		MaxValueInHistory = 0.0;
		double HeightScale = HeightScaleThroughput;
		ViewY += TextHeight;
		{ // draw graph edges + label
			const FLinearColor Color = FLinearColor::White;
			DrawLine(ViewX, ViewY + ViewHeight, ViewX + ViewWidth, ViewY + ViewHeight, Color, 1);
			DrawLine(ViewX, ViewY, ViewX, ViewY + ViewHeight, Color, 1);
			DrawLine(ViewX + ViewWidth, ViewY, ViewX + ViewWidth, ViewY + ViewHeight, Color, 1);
			DrawString(TEXT("ZenServer Throughput Mbps"), ViewX, ViewY + ViewHeight + 10, false);
		}

		for (int I = History.size() - 1; I >= 0; --I)
		{
			const auto& Item = History[I];
			const double       X = ViewX + ViewWidth - PixelsPerSecond * (StatsTimeNow - Item.Time);
			const double       H = FMath::Min(ViewHeight, ViewHeight * (Item.Throughput / HeightScale));
			const double       Y = ViewY + ViewHeight - H;
			const FLinearColor Color = FLinearColor::Yellow;
			
			DrawLine(X, ViewY + ViewHeight - 1, X, Y, Color, LineThickness);
			DrawString(FString::Printf(TEXT("%.2f"), Item.Throughput), X, Y - 11);

			if (Item.Throughput > MaxValueInHistory)
				MaxValueInHistory = Item.Throughput;
		}
		HeightScaleThroughput = FMath::Min(MaxHeightScaleThroughput, FMath::Max(MaxValueInHistory, 1.0));

		//SECOND GRAPH
		MaxValueInHistory = 0.0;
		ViewY += ViewHeight + (TextHeight * 2) ;
		HeightScale = HeightScaleRequest;

		{ // draw graph edges + label
			const FLinearColor Color = FLinearColor::White;
			DrawLine(ViewX, ViewY + ViewHeight, ViewX + ViewWidth, ViewY + ViewHeight, Color, 1);
			DrawLine(ViewX, ViewY, ViewX, ViewY + ViewHeight, Color, 1);
			DrawLine(ViewX + ViewWidth, ViewY, ViewX + ViewWidth, ViewY + ViewHeight, Color, 1);
			DrawString(TEXT("ZenServer Request/Sec Count"), ViewX, ViewY + ViewHeight + 10, false);
		}

		for (int I = History.size() - 1; I >= 0; --I)
		{
			const auto& Item = History[I];
			const double       X = ViewX + ViewWidth - PixelsPerSecond * (StatsTimeNow - Item.Time);
			const double       H = FMath::Min(ViewHeight, ViewHeight * (Item.RequestCount / HeightScale));
			const double       Y = ViewY + ViewHeight - H;
			const FLinearColor Color = FLinearColor::Gray;

			DrawLine(X, ViewY + ViewHeight - 1, X, Y, Color, LineThickness);
			DrawString(FString::Printf(TEXT("%.d"), Item.RequestCount), X, Y - 11);

			if (Item.RequestCount > MaxValueInHistory)
				MaxValueInHistory = Item.RequestCount;
		}
		HeightScaleRequest = FMath::Min(MaxHeightScaleRequest, FMath::Max(MaxValueInHistory, 1.0));
	}
}

void UStorageServerConnectionDebug::ShowGraph(FOutputDevice&)
{
	ShowGraphs = true;
}

void UStorageServerConnectionDebug::HideGraph(FOutputDevice&)
{
	ShowGraphs = false;
}

static FAutoConsoleCommandWithOutputDevice
	GShowDebugConnectionStatsCmd(
		TEXT("r.ZenServerStatsShow"),
		TEXT("Show ZenServer Stats Graph."),
		FConsoleCommandWithOutputDeviceDelegate::CreateStatic(&UStorageServerConnectionDebug::ShowGraph));

static FAutoConsoleCommandWithOutputDevice
	GHideDebugConnectionStatsCmd(
		TEXT("r.ZenServerStatsHide"),
		TEXT("Hide ZenServer Stats Graph."),
		FConsoleCommandWithOutputDeviceDelegate::CreateStatic(&UStorageServerConnectionDebug::HideGraph));


class FStorageServerClientDebugModule
	: public IModuleInterface
{
public:
#if !UE_BUILD_SHIPPING
	virtual void StartupModule() override
	{
		FCoreDelegates::OnPostEngineInit.AddLambda([this]
		{
			if (IStorageServerPlatformFile* StorageServerPlatformFile = IStorageServerClientModule::FindStorageServerPlatformFile())
			{
				ConnectionDebug = NewObject<UStorageServerConnectionDebug>();
				ConnectionDebug->SetPlatformFile(StorageServerPlatformFile);
				ConnectionDebug->AddToRoot();
				ConnectionDebug->StartDrawing();
			}
		});
	}

	virtual void ShutdownModule() override
	{
		if (ConnectionDebug)
		{
			ConnectionDebug->SetPlatformFile(nullptr);
			ConnectionDebug->StopDrawing();
			ConnectionDebug->RemoveFromRoot();
			ConnectionDebug = nullptr;
		}
	}
#endif // !UE_BUILD_SHIPPING

	UStorageServerConnectionDebug* ConnectionDebug = nullptr;
};

IMPLEMENT_MODULE(FStorageServerClientDebugModule, StorageServerClientDebug);
