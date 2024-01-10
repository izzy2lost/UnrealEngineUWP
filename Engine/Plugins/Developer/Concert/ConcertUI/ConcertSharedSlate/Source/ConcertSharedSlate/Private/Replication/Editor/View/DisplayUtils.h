// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

class FString;
class FText;
class UObject;

struct FConcertPropertyChain;
struct FSlateIcon;
struct FSoftObjectPath;

namespace UE::ConcertSharedSlate
{
	class IReplicationStreamModel;
}

namespace UE::ConcertSharedSlate::DisplayUtils
{
	/** @return The text to use for displaying this object's name */
	FText GetObjectDisplayText(const FSoftObjectPath& Object);
	
	/** @return More lightweight version of GetObjectDisplayText which does not construct any FText. */
	FString GetObjectDisplayString(const FSoftObjectPath& Object);
	/** @return More lightweight version of GetObjectDisplayText which does not construct any FText. */
	FString GetObjectDisplayString(const UObject& Object);
	
	/** @return The text to use for displaying this object's type */
	FText GetObjectTypeText(const IReplicationStreamModel& Model, const FSoftObjectPath& Object);
	
	/** @return The icon to use for this object */
	FSlateIcon GetObjectIcon(const IReplicationStreamModel& Model, const FSoftObjectPath& Object);
	/** @return The icon to use for this object */
	FSlateIcon GetObjectIcon(UObject& Object);

	/** @return The text to use for displaying this property's name. */
	FText GetPropertyDisplayText(const FConcertPropertyChain& Property);
	/** @return More lightweight version of GetPropertyDisplayString which does not construct any FText. */
	FString GetPropertyDisplayString(const FConcertPropertyChain& Property);
}
