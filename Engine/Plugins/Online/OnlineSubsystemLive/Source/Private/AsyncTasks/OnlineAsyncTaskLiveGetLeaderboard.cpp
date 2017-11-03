// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "../OnlineSubsystemLivePrivatePCH.h"
#include "OnlineAsyncTaskLiveGetLeaderboard.h"
#include "../OnlineLeaderboardInterfaceLive.h"
#include "../OnlineSubsystemLiveTypes.h"

#include <collection.h>

FOnlineAsyncTaskLiveGetLeaderboard::FOnlineAsyncTaskLiveGetLeaderboard(
	FOnlineSubsystemLive* InLiveSubsystem,
	LeaderboardResult^ InResults,
	const FOnlineLeaderboardReadRef& InReadObject,
	bool InFireDelegates)
	: LiveSubsystem(InLiveSubsystem),
	Results(InResults),
	ReadObject(InReadObject),
	bFireDelegates(InFireDelegates)
{
}

void FOnlineAsyncTaskLiveGetLeaderboard::Finalize()
{
	//Copy row data from the LeaderboardResults^ to the FOnlineLeaderboardRead
	for(unsigned int i = 0; i < Results->Rows->Size; i++)
	{
		LeaderboardRow^ LiveRow = Results->Rows->GetAt(i);
		FOnlineStatsRow NewStatsRow = FOnlineStatsRow(LiveRow->Gamertag->Data(), MakeShared<FUniqueNetIdLive>(LiveRow->XboxUserId));
		NewStatsRow.Rank = LiveRow->Rank;

		//Copy each column name and value into the new stats row
		for(int j = 0; j < ReadObject->ColumnMetadata.Num(); j++)
		{
			FVariantData Value = ConvertLeaderboardRowDataToRequestedType(LiveRow->Values->GetAt(j)->Data(), Results->Columns->GetAt(j)->StatisticType, ReadObject->ColumnMetadata[j].DataType);
			NewStatsRow.Columns.Add(Results->Columns->GetAt(j)->StatisticName->Data(), Value);
		}

		ReadObject->Rows.Add(NewStatsRow);
	}

	ReadObject->ReadState = EOnlineAsyncTaskState::Done;
}

void FOnlineAsyncTaskLiveGetLeaderboard::TriggerDelegates() 
{
	if(bFireDelegates)
	{
		FOnlineLeaderboardsLivePtr Leaderboards = LiveSubsystem->GetLeaderboardsInterfaceLive();
		Leaderboards->TriggerOnLeaderboardReadCompleteDelegates(true);
	}
}

FVariantData FOnlineAsyncTaskLiveGetLeaderboard::ConvertLeaderboardRowDataToRequestedType(const TCHAR* RowData, Windows::Foundation::PropertyType FromType, EOnlineKeyValuePairDataType::Type ToType)
{
	FString DataString(RowData);

	switch (ToType)
	{
	case EOnlineKeyValuePairDataType::Int32:
		{
			if(FromType != Windows::Foundation::PropertyType::Int32)
			{
				ReportTypeMismatchWarning(FromType, ToType);
			}
			int32 Value = FCString::Atoi(*DataString);
			return FVariantData(Value);
		}
	case EOnlineKeyValuePairDataType::Int64:
		{
			if(FromType != Windows::Foundation::PropertyType::Int64)
			{
				ReportTypeMismatchWarning(FromType, ToType);
			}
			else
			{
				//FVariantData does not support int64 it only supports uint64. The requested EOnlineKeyValuePairDataType
				//and the returned PropertyType both indicate int64 so this is a warning to call out a potential loss of data.
				UE_LOG_ONLINE(Warning, TEXT("Storing leaderboard result data (int64) as uint64. Possible loss of data."));
			}
			uint64 Value = FCString::Strtoui64(*DataString, NULL, 10);
			return FVariantData(Value);
		}
	case EOnlineKeyValuePairDataType::Double:
		{
			if(FromType != Windows::Foundation::PropertyType::Double)
			{
				ReportTypeMismatchWarning(FromType, ToType);
			}
			double Value = FCString::Atod(*DataString);
			return FVariantData(Value);
		}
	case EOnlineKeyValuePairDataType::String:
		{
			return FVariantData(DataString);
		}
	case EOnlineKeyValuePairDataType::Float:
		{
			if(FromType != Windows::Foundation::PropertyType::Single)
			{
				ReportTypeMismatchWarning(FromType, ToType);
			}
			float Value = FCString::Atof(*DataString);
			return FVariantData(Value);
		}
	case EOnlineKeyValuePairDataType::Bool:
		{
			if(FromType != Windows::Foundation::PropertyType::Boolean)
			{
				ReportTypeMismatchWarning(FromType, ToType);
			}
			bool Value = FCString::ToBool(*DataString);
			return FVariantData(Value);
		}
	default:
		UE_LOG_ONLINE(Warning, TEXT("Unsupported leaderboard result conversion."));
		return FVariantData();
	}
}

void FOnlineAsyncTaskLiveGetLeaderboard::ReportTypeMismatchWarning(Windows::Foundation::PropertyType FromType, EOnlineKeyValuePairDataType::Type ToType)
{
	FString FromTypeString;
	FString ToTypeString;
	//These are the currently supported PropertyTypes for data in a leaderboard.
	switch (FromType)
	{
	case Windows::Foundation::PropertyType::Int32:
		FromTypeString = "int32";
		break;
	case Windows::Foundation::PropertyType::UInt32:
		FromTypeString = "uint32";
		break;
	case Windows::Foundation::PropertyType::Int64:
		FromTypeString = "int64";
		break;
	case Windows::Foundation::PropertyType::UInt64:
		FromTypeString = "uint64";
		break;
	case Windows::Foundation::PropertyType::Single:
		FromTypeString = "single";
		break;
	case Windows::Foundation::PropertyType::Double:
		FromTypeString = "double";
		break;
	case Windows::Foundation::PropertyType::String:
		FromTypeString = "string";
		break;
	case Windows::Foundation::PropertyType::Boolean:
		FromTypeString = "bool";
		break;
	case Windows::Foundation::PropertyType::Guid:
		FromTypeString = "GUID";
		break;
	default:
		FromTypeString = "Unknown";
	}
	
	switch (ToType)
	{
	case EOnlineKeyValuePairDataType::Int32:
		ToTypeString = "int32";
		break;
	case EOnlineKeyValuePairDataType::Int64:
		ToTypeString = "int64";
		break;
	case EOnlineKeyValuePairDataType::Double:
		ToTypeString = "double";
		break;
	case EOnlineKeyValuePairDataType::String:
		ToTypeString = "string";
		break;
	case EOnlineKeyValuePairDataType::Float:
		ToTypeString = "float";
		break;
	case EOnlineKeyValuePairDataType::Bool:
		ToTypeString = "bool";
		break;
	default:
		ToTypeString = "Unknown";
	}
	
	UE_LOG_ONLINE(Warning, TEXT("Converting leaderboard result data from '%s' to '%s'"), *FromTypeString, *ToTypeString);
}

FOnlineAsyncTaskLiveGetLeaderboardFailed::FOnlineAsyncTaskLiveGetLeaderboardFailed(FOnlineSubsystemLive* InLiveSubsystem) : LiveSubsystem(InLiveSubsystem)
{
}

void FOnlineAsyncTaskLiveGetLeaderboardFailed::TriggerDelegates() 
{
		FOnlineLeaderboardsLivePtr Leaderboards = LiveSubsystem->GetLeaderboardsInterfaceLive();
		Leaderboards->TriggerOnLeaderboardReadCompleteDelegates(false);
}
