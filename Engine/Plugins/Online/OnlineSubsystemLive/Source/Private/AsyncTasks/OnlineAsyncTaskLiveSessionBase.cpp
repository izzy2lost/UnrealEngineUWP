// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "../OnlineSubsystemLivePrivatePCH.h"
#include "OnlineAsyncTaskLiveSessionBase.h"
#include "OnlineSubsystemSessionSettings.h"
#include "OnlineSubsystemLive.h"
#include "../OnlineSessionInterfaceLive.h"

#include <collection.h>

using namespace Platform;
using namespace Microsoft::Xbox::Services;
using namespace Microsoft::Xbox::Services::Multiplayer;
using namespace Microsoft::Xbox::Services::Matchmaking;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Xbox::Networking;

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

FOnlineAsyncTaskLiveSessionBase::FOnlineAsyncTaskLiveSessionBase(
class FOnlineSubsystemLive* InLiveSubsystem,
	int32		InUserIndex,
	FName		InSessionName, 
	const FOnlineSessionSettings& NewSessionSettings
	)
	: FOnlineAsyncTaskLive( InLiveSubsystem, InUserIndex )
	, SessionSettings( NewSessionSettings )
	, SessionName( InSessionName )
	, SessionTemplateName()
	, SessionMaxSeats( 0 )
	, ClientMatchmakingCapable( false )
	, CustomConstantsJson( ref new String( L"{}" ) )
	, InitiatorXboxUserIds( ref new Platform::Collections::Vector<Platform::String^> )
	, PartyEnabledSession( true )
	, LiveSession (nullptr)
{
	ParseSessionSettings( &SessionSettings );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

FOnlineAsyncTaskLiveSessionBase::FOnlineAsyncTaskLiveSessionBase( FOnlineAsyncTaskLiveSessionBase* PreviousTask )
	: FOnlineAsyncTaskLive( PreviousTask->Subsystem, PreviousTask->UserIndex )
{
	// Copy entire previous task data
	*this = *PreviousTask;

	//. But we are not complete or even started yet!
	bWasSuccessful = false;
	bIsComplete = false;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

FOnlineAsyncTaskLiveSessionBase::~FOnlineAsyncTaskLiveSessionBase()
{
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

void FOnlineAsyncTaskLiveSessionBase::ParseSessionSettings( FOnlineSessionSettings* InSessionSettings )
{
	if ( InSessionSettings )
	{
		SessionMaxSeats = InSessionSettings->NumPublicConnections;

		//const FOnlineSessionSetting* HopperSetting = InSessionSettings->Settings.Find( SETTING_MATCHING_HOPPER );
		//if ( HopperSetting )
		//{
		//	HopperSetting->Data.GetValue( MatchHopperName );
		//}

		const FOnlineSessionSetting* TimeoutSetting = InSessionSettings->Settings.Find( SETTING_MATCHING_TIMEOUT );
		if ( TimeoutSetting )
		{
			TimeoutSetting->Data.GetValue( MatchTimeout );
		}

		const FOnlineSessionSetting* AttributesSetting = InSessionSettings->Settings.Find( SETTING_MATCHING_ATTRIBUTES );
		if ( AttributesSetting )
		{
			AttributesSetting->Data.GetValue( MatchTicketAttributes );
		}

		const FOnlineSessionSetting* TemplateSetting = InSessionSettings->Settings.Find( SETTING_GAMEMODE );
		if ( TemplateSetting )
		{
			TemplateSetting->Data.GetValue( SessionTemplateName );
		}

		const FOnlineSessionSetting* CustomJsonSetting = InSessionSettings->Settings.Find( SETTING_CUSTOM );
		if ( CustomJsonSetting )
		{
			FString InJson;

			CustomJsonSetting->Data.GetValue( InJson );

			if ( InJson.Len() )
			{
				CustomConstantsJson = ref new String( *InJson );
			}
		}

		const FOnlineSessionSetting* PartyEnabledSessionSetting = InSessionSettings->Settings.Find( SETTING_PARTY_ENABLED_SESSION );
		if( PartyEnabledSessionSetting )
		{
			PartyEnabledSessionSetting->Data.GetValue( PartyEnabledSession );
		}
	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

void FOnlineAsyncTaskLiveSessionBase::ParseSearchSettings( const FOnlineSessionSearch* InSearchSettings )
{
	if ( InSearchSettings )
	{
		InSearchSettings->QuerySettings.Get(SEARCH_XBOX_LIVE_HOPPER_NAME, MatchHopperName);
		InSearchSettings->QuerySettings.Get(SEARCH_XBOX_LIVE_SESSION_TEMPLATE_NAME, SessionTemplateName);
		MatchTimeout = InSearchSettings->TimeoutInSeconds;
	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

bool FOnlineAsyncTaskLiveSessionBase::SettingsAreValid()
{
	if (	MatchTimeout
		&&  SessionTemplateName.Len()
		&&  MatchHopperName.Len() )
	{
		return true;
	}

	return false;
}

//------------------------------- End of file ---------------------------------
