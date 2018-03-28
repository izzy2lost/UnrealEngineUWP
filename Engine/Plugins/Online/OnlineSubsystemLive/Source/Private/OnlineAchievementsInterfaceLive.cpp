// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemLivePrivatePCH.h"
#include "OnlineAchievementsInterfaceLive.h"
#include "OnlineEventsInterfaceLive.h"
#include "OnlineSubsystemLiveTypes.h"
#include "OnlineSubsystemLive.h"
#include "OnlineIdentityInterfaceLive.h"
#include "OnlineAsyncTaskManagerLive.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/CString.h"

#define TEST_ACHIEVEMENTS			0
#define USE_EVENTS_HEADER_TEST		0

#if USE_EVENTS_HEADER_TEST
	#include "Events-EPCC.1-45783947.h"
#endif

// @ATG_CHANGE : UWP LIVE support: Xbox headers to pch

using namespace Microsoft::Xbox::Services::Achievements;
using namespace Microsoft::Xbox::Services::RealTimeActivity;
using namespace Microsoft::Xbox::Services;
using namespace Microsoft::Xbox::Services::UserStatistics;
using namespace Windows::Foundation;

FOnlineAchievementsLive::FOnlineAchievementsLive( class FOnlineSubsystemLive* InSubsystem ) : LiveSubsystem( InSubsystem )
{
	LoadAndInitFromJsonConfig( TEXT( "Achievements.json" ) );

#if TEST_ACHIEVEMENTS		// Enable this to test achievements and events
	TestEventsAndAchievements();
#endif
}

FOnlineAchievementsLive::~FOnlineAchievementsLive()
{
}

bool FOnlineAchievementsLive::LoadAndInitFromJsonConfig( const TCHAR* JsonConfigName )
{
	const FString BaseDir = FPaths::ProjectDir() + TEXT( "Config/OSS/Live/" );
	const FString JSonConfigFilename = BaseDir + JsonConfigName;;

	FString JSonText;

	if ( !FFileHelper::LoadFileToString( JSonText, *JSonConfigFilename ) )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineAchievementsLive: Failed to find json OSS achievements config: %s"), *JSonConfigFilename );
		return false;
	}

	if ( !AchievementsConfig.FromJson( JSonText ) )
	{
		UE_LOG_ONLINE( Warning, TEXT( "FOnlineAchievementsLive: Failed to parse json OSS achievements config: %s"), *JSonConfigFilename );
		return false;
	}

	return true;
}

void FOnlineAchievementsLive::TestEventsAndAchievements()
{
	FOnlineIdentityLivePtr Identity = LiveSubsystem->GetIdentityLive();

	check(Identity.IsValid());

	FString PlayerXUIDStr;
	Microsoft::Xbox::Services::XboxLiveContext^ xboxLiveContext = nullptr;
	{
		Windows::Xbox::System::User ^ TestUser = Identity->GetCachedUsers()->GetAt(0);
		if (TestUser != nullptr)
		{
			xboxLiveContext = LiveSubsystem->GetLiveContext(TestUser);
			if (xboxLiveContext != nullptr)
			{
				PlayerXUIDStr = FString(TestUser->XboxUserId->Data());
			}
		}

		if ((PlayerXUIDStr.IsEmpty()) || (xboxLiveContext == nullptr))
		{
			return;
		}
	}
	
	// Turn on debug logging to Output debug window for Xbox Services
	xboxLiveContext->Settings->DiagnosticsTraceLevel = XboxServicesDiagnosticsTraceLevel::Verbose;

	// Show service calls from Xbox Services on the UI for easy debugging
	xboxLiveContext->Settings->EnableServiceCallRoutedEvents = true;
	xboxLiveContext->Settings->ServiceCallRouted += ref new Windows::Foundation::EventHandler<Microsoft::Xbox::Services::XboxServiceCallRoutedEventArgs^>( 
		[=]( Platform::Object^, Microsoft::Xbox::Services::XboxServiceCallRoutedEventArgs^ args )
	{
		//if( args->HttpStatus != 200 )
		{
			UE_LOG_ONLINE( Warning, TEXT( "[URL]: %s %s"), args->HttpMethod->Data(), args->Url->AbsoluteUri->Data() );
			if( !args->RequestBody->RequestMessageString->IsEmpty() )
			{
				UE_LOG_ONLINE( Warning, TEXT( "[RequestBody]: %s"), args->RequestBody->RequestMessageString->Data() );
			}
			UE_LOG_ONLINE( Warning, TEXT( "") );
			UE_LOG_ONLINE( Warning, TEXT( "[Response]: %s %s"), args->HttpStatus.ToString()->Data(), args->ResponseBody->Data() );
			UE_LOG_ONLINE( Warning, TEXT( "") );
		}
	});

	xboxLiveContext->UserStatisticsService->StatisticChanged += ref new Windows::Foundation::EventHandler<StatisticChangeEventArgs^>(
		[]( Platform::Object^, StatisticChangeEventArgs^ args )
	{
		UE_LOG_ONLINE( Warning, TEXT( "Stat User: %s"), args->XboxUserId->Data() );
		UE_LOG_ONLINE( Warning, TEXT( "Stat Name: %s"), args->LatestStatistic->StatisticName->Data() );
		UE_LOG_ONLINE( Warning, TEXT( "Stat Value: %s"), args->LatestStatistic->Value->Data() );
	});

#if USE_EVENTS_HEADER_TEST
	if ( EventRegisterEPCC_45783947() != ERROR_SUCCESS )
	{
		return;
	}

	GUID PlayerSessionId = { 1 };

	uint32 Result = 0;

	Result = EventWritePlayerSessionStart( *PlayerXUIDStr, &PlayerSessionId, NULL, 0, 0 );

	if ( Result != ERROR_SUCCESS )
	{
		return;
	}

	Result = EventWriteTempActivateAchiement( *PlayerXUIDStr, &PlayerSessionId, 10 );

	if ( Result != ERROR_SUCCESS )
	{
		return;
	}

	Result = EventWritePlayerSessionEnd( *PlayerXUIDStr, &PlayerSessionId, NULL, 0, 0, 0 );

	if ( Result != ERROR_SUCCESS )
	{
		return;
	}

	EventUnregisterEPCC_45783947();
#else
	FOnlineEventsLive * EventInterface = (FOnlineEventsLive*)LiveSubsystem->GetEventsInterface().Get();

	FUniqueNetIdLive PlayerId( PlayerXUIDStr );

	// Start player session
	{
		FOnlineEventParms Parms;

		Parms.Add( TEXT( "GameplayModeId" ), FVariantData( (int32)1 ) );
		Parms.Add( TEXT( "DifficultyLevelId" ), FVariantData( (int32)1 ) );
		Parms.Add( TEXT( "MapName" ), FVariantData( FString("Highrise") ) );

		EventInterface->TriggerEvent( PlayerId, TEXT( "PlayerSessionStart" ), Parms );
	}

	// Test a stat change event
	{
		FOnlineEventParms Parms;

		Parms.Add( TEXT( "SectionId" ), FVariantData( (int32)0 ) );
		Parms.Add( TEXT( "GameplayModeId" ), FVariantData( (int32)0 ) );
		Parms.Add( TEXT( "DifficultyLevelId" ), FVariantData( (int32)0 ) );
		Parms.Add( TEXT( "PlayerRoleId" ), FVariantData( (int32)0 ) );
		Parms.Add( TEXT( "PlayerWeaponId" ), FVariantData( (int32)0 ) );
		Parms.Add( TEXT( "EnemyRoleId" ), FVariantData( (int32)0 ) );
		Parms.Add( TEXT( "KillTypeId" ), FVariantData( (int32)0 ) );
		Parms.Add( TEXT( "LocationX" ), FVariantData( (float)0 ) );
		Parms.Add( TEXT( "LocationY" ), FVariantData( (float)0 ) );
		Parms.Add( TEXT( "LocationZ" ), FVariantData( (float)0 ) );
		Parms.Add( TEXT( "EnemyWeaponId" ), FVariantData( (int32)0 ) );

		EventInterface->TriggerEvent( PlayerId, TEXT( "KillOponent" ), Parms );
	}

	// Give test achievement
	{
		FOnlineEventParms Parms;
		Parms.Add( TEXT( "AchievementIndex" ), FVariantData( (int32)9 ) );
		//Parms.Add( TEXT( "AchievementIndex" ), FVariantData( (uint64)0xFFFFFFFFF ) );				// This should trigger loss of data error
		//Parms.Add( TEXT( "AchievementIndex" ), FVariantData( FString( TEXT( "Test") ) ) );		// This should trigger conversion error

		EventInterface->TriggerEvent( PlayerId, TEXT( "TempActivateAchiement" ), Parms );
	}

	// End player session
	{
		FOnlineEventParms Parms;

		Parms.Add( TEXT( "GameplayModeId" ), FVariantData( (int32)1 ) );
		Parms.Add( TEXT( "DifficultyLevelId" ), FVariantData( (int32)1 ) );
		Parms.Add( TEXT( "ExitStatusId" ), FVariantData( (int32)0 ) );
		Parms.Add( TEXT( "MapName" ), FVariantData( FString("Highrise") ) );
		Parms.Add( TEXT( "PlayerScore" ), FVariantData( (int32)0 ) );
		Parms.Add( TEXT( "PlayerWon" ), FVariantData( (bool)false ) );

		EventInterface->TriggerEvent( PlayerId, TEXT( "PlayerSessionEnd" ), Parms );
	}
#endif
}

void FOnlineAchievementsLive::WriteAchievements(const FUniqueNetId& PlayerId, FOnlineAchievementsWriteRef& WriteObject, const FOnAchievementsWrittenDelegate& Delegate)
{
// @ATG_CHANGE : BEGIN - Achievements 2017 support
#if USE_ACHIEVEMENTS_2017
	if (!LiveSubsystem)
	{
		UE_LOG_ONLINE(Warning, TEXT("The Live Subsystem has not been initialized."));
		WriteObject->WriteState = EOnlineAsyncTaskState::Failed;
		Delegate.ExecuteIfBound(PlayerId, false);
		return;
	}

	const FOnlineIdentityLivePtr Identity = LiveSubsystem->GetIdentityLive();
	if (!Identity.IsValid())
	{
		UE_LOG_ONLINE(Warning, TEXT("The Live Subsystem Identity interface is invalid."));
		WriteObject->WriteState = EOnlineAsyncTaskState::Failed;
		Delegate.ExecuteIfBound(PlayerId, false);
		return;
	}

	const FUniqueNetIdLive UserLive(PlayerId);
	Windows::Xbox::System::User^ XBoxUser = Identity->GetUserForUniqueNetId(UserLive);
	if (!XBoxUser)
	{
		UE_LOG_ONLINE(Warning, TEXT("Could not get Xbox Live user for the given Player Id."));
		WriteObject->WriteState = EOnlineAsyncTaskState::Failed;
		Delegate.ExecuteIfBound(PlayerId, false);
		return;
	}

	Microsoft::Xbox::Services::XboxLiveContext^ LiveContext = LiveSubsystem->GetLiveContext(XBoxUser);
	if (!LiveContext)
	{
		UE_LOG_ONLINE(Warning, TEXT("Could not get the Xbox Live Context for the given Xbox Live user."));
		WriteObject->WriteState = EOnlineAsyncTaskState::Failed;
		Delegate.ExecuteIfBound(PlayerId, false);
		return;
	}

	bool bSyncResult = true;
	TArray<concurrency::task<void>> AchievementUpdates;
	AchievementUpdates.Empty(WriteObject->Properties.Num());
	WriteObject->WriteState = EOnlineAsyncTaskState::InProgress;

	for (FStatPropertyArray::TConstIterator It(WriteObject->Properties); It; ++It)
	{
		FName AchId = It.Key();
		Platform::String^ AchIdStr = ref new Platform::String(*AchId.ToString());

		float Percent = 0.0f;
		It.Value().GetValue(Percent);
		// Clamp as there's a WinRT exception if the percentage is too high
		Percent = FMath::Clamp(Percent, 0.0f, 100.0f);

		try
		{
			IAsyncAction^ pAsyncOp = LiveContext->AchievementService->UpdateAchievementAsync(
				XBoxUser->XboxUserId,   // The Xbox User ID of the player.
				AchIdStr,               // The achievement ID as defined by XDP or Dev Center.
				(uint32)Percent		    // The completion percentage of the achievement to indicate progress.
			);
			AchievementUpdates.Emplace(concurrency::create_task(pAsyncOp));
		}
		catch (Platform::Exception ^ Ex)
		{
			bSyncResult = false;
			UE_LOG_ONLINE(Warning, TEXT("UpdateAchievementAsync failed synchronously. Exception: %s."), Ex->ToString()->Data());
		}
	}

	if (AchievementUpdates.Num() > 0)
	{
		// Need to monitor the tasks even if we've already failed in case there's a further async failure -
		// otherwise we can have a fatal unobserved exception.
		concurrency::create_task([=]()
		{
			bool bAsyncResult = true;

			// Observe the individual tasks
			for (concurrency::task<void> IndividualUpdate : AchievementUpdates)
			{
				try
				{
					IndividualUpdate.get();
				}
				catch (Platform::Exception^ Ex)
				{
					UE_LOG_ONLINE(Warning, TEXT("UpdateAchievementAsync failed asynchronously. Exception: %s."), Ex->ToString()->Data());
					bAsyncResult = false;
				}
			}

			bool bResult = bSyncResult && bAsyncResult;
			WriteObject->WriteState = bResult ? EOnlineAsyncTaskState::Done : EOnlineAsyncTaskState::Failed;

			LiveSubsystem->ExecuteNextTick([Delegate, UserLive, bResult]()
			{
				Delegate.ExecuteIfBound(UserLive, bResult);
			});
		});
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("No achievements were written."));
		WriteObject->WriteState = EOnlineAsyncTaskState::Failed;
		Delegate.ExecuteIfBound(PlayerId, false);
	}
#else
// @ATG_CHANGE : END

	FOnlineEventsLive * EventInterface = (FOnlineEventsLive*)LiveSubsystem->GetEventsInterface().Get();

	FUniqueNetIdLive LiveId( PlayerId );

	if ( LiveId.UniqueNetIdStr.Len() <= 1 )
	{
		Delegate.ExecuteIfBound(PlayerId, false);
		return;
	}

	bool bResult = true;

	const FOnlineIdentityLivePtr Identity = LiveSubsystem->GetIdentityLive();
	check(Identity.IsValid());
	Windows::Xbox::System::User^ XBoxUser = Identity->GetUserForUniqueNetId(LiveId);
	Microsoft::Xbox::Services::XboxLiveContext ^ LiveContext = LiveSubsystem->GetLiveContext(XBoxUser);

	for (FStatPropertyArray::TConstIterator It(WriteObject->Properties); It; ++It)
	{
		float Percent = 0.0f;
		It.Value().GetValue(Percent);

		// Clamp as there's a WinRT exception if the percentage is too high
		Percent = FMath::Clamp(Percent, 0.0f, 100.0f);

		// The XBL back end wants the achievement ID, which is the number assigned to the achievement
		// This is the the order in which the achievements are created on XDP/UDC, starting from 1
		int32* AchievementId = AchievementsConfig.AchievementMap.Find(It.Key().ToString());

		if (AchievementId == NULL)
		{
			UE_LOG_ONLINE(Warning, TEXT("FOnlineAchievementsLive::WriteAchievements: No mapping for achievement %s"), *It.Key().ToString());
			bResult = false;
			continue;
		}

		// Then for unknown reasons, UpdateAchievementAsync wants this ID as a string
		FString AchievementIdStr = FString::FromInt(*AchievementId);

		Platform::String^ NetIdPlatformStr = ref new Platform::String(*LiveId.UniqueNetIdStr);
		Platform::String^ AchievementPlatformStr = ref new Platform::String(*AchievementIdStr);

		try
		{
			LiveContext->AchievementService->UpdateAchievementAsync(NetIdPlatformStr, AchievementPlatformStr, (uint32)Percent);
		}
		catch (Platform::COMException^ Ex)
		{
			UE_LOG_ONLINE(Warning, TEXT("UpdateAchievementAsync failed. Exception: %s."), Ex->ToString()->Data());
			bResult = false;
		}

		if ( Percent < 100.0f )
		{
			continue;
		}

		FName Name = It.Key();

		int32* Index = AchievementsConfig.AchievementMap.Find( Name.ToString() );

		if ( Index == NULL )
		{
			UE_LOG_ONLINE( Warning, TEXT( "FOnlineAchievementsLive::WriteAchievements: No mapping for achievement %s"), *Name.ToString() );
			bResult = false;
			continue;
		}

		FOnlineEventParms Parms;
		Parms.Add( TEXT( "AchievementIndex" ), FVariantData( (int32)*Index ) );

		if ( !EventInterface->TriggerEvent( PlayerId, *AchievementsConfig.AchievementEventName, Parms ) )
		{
			bResult = false;
		}
	}

	//@TODO: This is probably too soon, and should be pushed thru to happen once the event has finished processing
	Delegate.ExecuteIfBound(PlayerId, bResult);

// @ATG_CHANGE : BEGIN - Achievements 2017 support
#endif
// @ATG_CHANGE : END
};

void FOnlineAchievementsLive::QueryAchievements( const FUniqueNetId& PlayerId, const FOnQueryAchievementsCompleteDelegate& Delegate )
{
	if ( !LiveSubsystem )
	{
		Delegate.ExecuteIfBound( PlayerId, false );
		return;
	}

	const auto Identity = LiveSubsystem->GetIdentityLive();

	if ( !Identity.IsValid() )
	{
		Delegate.ExecuteIfBound( PlayerId, false );
		return;
	}

	const FUniqueNetIdLive UserLive( PlayerId );

	Windows::Xbox::System::User^ XBoxUser = Identity->GetUserForUniqueNetId( UserLive );

	if ( !XBoxUser )
	{
		Delegate.ExecuteIfBound( PlayerId, false );
		return;
	}

	try
	{
		Microsoft::Xbox::Services::XboxLiveContext ^ LiveContext = LiveSubsystem->GetLiveContext( XBoxUser );

		auto pAsyncOp = LiveContext->AchievementService->GetAchievementsForTitleIdAsync(
			XBoxUser->XboxUserId,						// Xbox LIVE user Id
			LiveSubsystem->TitleId,						// Title Id to get achievement data for
			AchievementType::All,						// AchievementType filter: All mean to get Persistent and Challenge achievements
			false,										// All possible achievements including accurate unlocked data
			AchievementOrderBy::TitleId,				// AchievementOrderBy filter: Default means no particular order
			0,											// The number of achievement items to skip
			0											// The maximum number of achievement items to return in the response
		);

		concurrency::create_task( pAsyncOp ).then( [this, UserLive, Delegate]( concurrency::task<AchievementsResult ^> Task )
		{
			try
			{
				auto Results = Task.get();

				Platform::Collections::Vector<Achievement^>^ AllAchievements = ref new Platform::Collections::Vector<Achievement ^>();
				ProcessGetAchievementsResults(Results, AllAchievements, UserLive, Delegate);
			}
			catch (Platform::COMException^ Ex)
			{
				UE_LOG_ONLINE(Warning, TEXT( "Getting achievements failed. Exception: %s." ), Ex->ToString()->Data() );
				LiveSubsystem->ExecuteNextTick([Delegate, UserLive]()
				{
					Delegate.ExecuteIfBound(UserLive, false );
				});
			}
		});
	}
	catch ( Platform::Exception ^ Ex )
	{
		if ( Ex->HResult != INET_E_DATA_NOT_AVAILABLE )
		{
			UE_LOG_ONLINE(Warning, TEXT( "Getting achievements failed. Exception: %s." ), Ex->ToString()->Data() );
			Delegate.ExecuteIfBound( PlayerId, false );
		}
	}
}

void FOnlineAchievementsLive::ProcessGetAchievementsResults(AchievementsResult^ Results, Platform::Collections::Vector<Achievement^>^ AllAchievements, const FUniqueNetIdLive UserLive, const FOnQueryAchievementsCompleteDelegate Delegate)
{
	if (Results != nullptr && Results->Items != nullptr )
	{
		uint32 ItemCount = Results->Items->Size;
		for (uint32 i = 0; i < ItemCount; ++i)
		{
			AllAchievements->Append(Results->Items->GetAt(i));
		}
		if (Results->HasNext)
		{
			auto pAsyncOp = Results->GetNextAsync(0);
			concurrency::create_task(pAsyncOp).then([this, UserLive, Delegate, AllAchievements](concurrency::task<AchievementsResult^> Task)
			{
				try
				{
					auto NextResults = Task.get();

					ProcessGetAchievementsResults(NextResults, AllAchievements, UserLive, Delegate);
				}
				catch (Platform::COMException^ Ex)
				{
					UE_LOG_ONLINE(Warning, TEXT( "Getting achievements failed. Exception: %s." ), Ex->ToString()->Data() );
					LiveSubsystem->ExecuteNextTick([Delegate, UserLive]()
					{
						Delegate.ExecuteIfBound(UserLive, false );
					});
				}
			});
		}
		else
		{
			LiveSubsystem->CreateAndDispatchAsyncEvent<FAsyncEventQueryCompleted>(LiveSubsystem, UserLive, AllAchievements, true, Delegate);
		}
	}
}

void FOnlineAchievementsLive::QueryAchievementDescriptions( const FUniqueNetId& PlayerId, const FOnQueryAchievementsCompleteDelegate& Delegate )
{
	// Just query achievements to get descriptions
	// FIXME: This feels a little redundant, but we can see how platforms evolve, and make a decision then
	QueryAchievements( PlayerId, Delegate );
}

EOnlineCachedResult::Type FOnlineAchievementsLive::GetCachedAchievement(const FUniqueNetId& PlayerId, const FString& AchievementId, FOnlineAchievement& OutAchievement)
{
	TArray< FOnlineAchievement > * Achievements = PlayerAchievements.Find( FUniqueNetIdLive( PlayerId ) );

	if ( Achievements == NULL )
	{
		UE_LOG_ONLINE( Warning, TEXT( "XBoxOne achievements have not been read for player %s"), *PlayerId.ToString() );
		return EOnlineCachedResult::NotFound;
	}

	// Look up platform ID from achievement mapping
	int32* Index = AchievementsConfig.AchievementMap.Find( AchievementId );
	if (Index == nullptr)
	{
		UE_LOG_ONLINE(Warning, TEXT("FOnlineAchievementsLive::GetCachedAchievement: No mapping for achievement %s"), *AchievementId);
		return EOnlineCachedResult::NotFound;
	}
	FString PlatformAchievementId = FString::FromInt(*Index);

	for ( int32 i = 0; i < Achievements->Num(); i++ )
	{
		if ( (*Achievements)[ i ].Id == PlatformAchievementId )
		{
			OutAchievement = (*Achievements)[ i ];
			return EOnlineCachedResult::Success;
		}
	}

	return EOnlineCachedResult::NotFound;
};

EOnlineCachedResult::Type FOnlineAchievementsLive::GetCachedAchievements(const FUniqueNetId& PlayerId, TArray<FOnlineAchievement> & OutAchievements)
{
	TArray< FOnlineAchievement > * Achievements = PlayerAchievements.Find( FUniqueNetIdLive( PlayerId ) );

	if ( Achievements == NULL )
	{
		UE_LOG_ONLINE( Warning, TEXT( "XBoxOne achievements have not been read for player %s"), *PlayerId.ToString() );
		return EOnlineCachedResult::NotFound;
	}

	OutAchievements = *Achievements;

	return EOnlineCachedResult::Success;
};

EOnlineCachedResult::Type FOnlineAchievementsLive::GetCachedAchievementDescription(const FString& AchievementId, FOnlineAchievementDesc& OutAchievementDesc)
{
	int32* Index = AchievementsConfig.AchievementMap.Find( AchievementId );
	if (Index == nullptr)
	{
		UE_LOG_ONLINE(Warning, TEXT("FOnlineAchievementsLive::GetCachedAchievementDescription: No mapping for achievement %s"), *AchievementId);
		return EOnlineCachedResult::NotFound;
	}
	FString PlatformAchievementId = FString::FromInt(*Index);

	FOnlineAchievementDesc * AchievementDesc = AchievementDescriptions.Find( PlatformAchievementId );

	if ( AchievementDesc == NULL )
	{
		UE_LOG_ONLINE( Warning, TEXT( "XBoxOne achievements have not been read for id: %s"), *AchievementId );
		return EOnlineCachedResult::NotFound;
	}

	OutAchievementDesc = *AchievementDesc;
	return EOnlineCachedResult::Success;
};

#if !UE_BUILD_SHIPPING
bool FOnlineAchievementsLive::ResetAchievements(const FUniqueNetId& PlayerId)
{
	return false;
};
#endif // !UE_BUILD_SHIPPING


FString FOnlineAchievementsLive::FAsyncEventQueryCompleted::ToString() const 
{
	return TEXT( "Query achievements complete." );
}

void FOnlineAchievementsLive::FAsyncEventQueryCompleted::TriggerDelegates()
{
	FOnlineAsyncEvent::TriggerDelegates();
	Delegate.ExecuteIfBound( PlayerId, bWasSuccessful );
}

void FOnlineAchievementsLive::FAsyncEventQueryCompleted::Finalize()
{
	FOnlineAsyncEvent::Finalize();

	if ( !Subsystem )
	{
		bWasSuccessful = false;
		return;
	}

	FOnlineAchievementsLivePtr SubSystemAchievements = StaticCastSharedPtr< FOnlineAchievementsLive >( Subsystem->GetAchievementsInterface() );

	if ( !SubSystemAchievements.IsValid() )
	{
		bWasSuccessful = false;
		return;
	}

	if ( Achievements == nullptr || Achievements->Size == 0)
	{
		bWasSuccessful = false;
		return;
	}

	TArray< FOnlineAchievement > AchievementsForPlayer;

	for ( uint32 i = 0; i < Achievements->Size; ++i )
	{
		Achievement^ XBoxAchievement = Achievements->GetAt(i);

		FOnlineAchievement OnlineAchievement; 

		// Copy over id
		OnlineAchievement.Id = XBoxAchievement->Id->Data();

		// FIXME: We can get analog progress here
		OnlineAchievement.Progress = ( XBoxAchievement->ProgressState == AchievementProgressState::Achieved ) ? 100 : 0;

		AchievementsForPlayer.Add( OnlineAchievement );

		FOnlineAchievementDesc Desc;

		// Fill in description
		Desc.Title			= FText::FromString( XBoxAchievement->Name->Data() );
		Desc.LockedDesc		= FText::FromString( XBoxAchievement->LockedDescription->Data() );
		Desc.UnlockedDesc	= FText::FromString( XBoxAchievement->UnlockedDescription->Data() );
		Desc.bIsHidden		= XBoxAchievement->IsSecret;
		Desc.UnlockTime		= FDateTime( 1601, 1, 1 ) + FTimespan( (int64)XBoxAchievement->Progression->TimeUnlocked.UniversalTime );

		// Should replace any already existing values
		SubSystemAchievements->AchievementDescriptions.Add( OnlineAchievement.Id, Desc );
	}

	// Should replace any already existing values
	SubSystemAchievements->PlayerAchievements.Add( PlayerId, AchievementsForPlayer );
}
