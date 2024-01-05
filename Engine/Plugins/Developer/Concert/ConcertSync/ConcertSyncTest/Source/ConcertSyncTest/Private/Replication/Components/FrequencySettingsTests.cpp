// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/Data/ReplicationFrequencySettings.h"

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

namespace UE::ConcertSyncTests
{
	/** Tests that FConcertObjectReplicationSettings's < and <= work as expected. */
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrequencySettingsLessEqual, "Editor.Concert.Replication.Components.FrequencySettingsLessEqual", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);
	bool FFrequencySettingsLessEqual::RunTest(const FString& Parameters)
	{
		constexpr FConcertObjectReplicationSettings FPS_30 { EConcertObjectReplicationMode::SpecifiedRate, 30 }; 
		constexpr FConcertObjectReplicationSettings FPS_60 { EConcertObjectReplicationMode::SpecifiedRate, 60 };
		// Note for realtime ReplicationRate is ignored - we're only putting  30 and 60 to test that they are also ignored for comparisons
		constexpr FConcertObjectReplicationSettings Realtime_20 { EConcertObjectReplicationMode::Realtime, 20 };
		constexpr FConcertObjectReplicationSettings Realtime_60 { EConcertObjectReplicationMode::Realtime, 60 };

		TestTrue(TEXT("FPS_30 <= FPS_30 == true"), FPS_30 <= FPS_30);
		TestTrue(TEXT("FPS_30 < FPS_60 == true"), FPS_30 < FPS_60);
		TestTrue(TEXT("FPS_30 <= FPS_60 == true"), FPS_30 <= FPS_60);
		TestFalse(TEXT("FPS_60 < FPS_30 == false"), FPS_60 < FPS_30);
		TestFalse(TEXT("FPS_60 <= FPS_30 == false"), FPS_60 <= FPS_30);
		
		TestTrue(TEXT("FPS_30 < FPS_60 == true"), FPS_30 < Realtime_20);
		TestTrue(TEXT("FPS_30 <= FPS_60 == true"), FPS_30 <= Realtime_20);
		TestFalse(TEXT("Realtime_20 < FPS_30 == false"), Realtime_20 < FPS_30);
		TestFalse(TEXT("Realtime_20 <= FPS_30 == false"), Realtime_20 <= FPS_30);
		
		TestTrue(TEXT("Realtime_20 <= Realtime_20 == true"), Realtime_20 <= Realtime_20);
		TestTrue(TEXT("Realtime_20 <= Realtime_60 == true"), Realtime_20 <= Realtime_60);
		TestTrue(TEXT("Realtime_60 <= Realtime_20 == true"), Realtime_60 <= Realtime_20);
		TestFalse(TEXT("Realtime_20 < Realtime_60 == false"), Realtime_20 < Realtime_60);
		TestFalse(TEXT("Realtime_60 < Realtime_20 == false"), Realtime_60 < Realtime_20);
		
		return true;
	}
}
