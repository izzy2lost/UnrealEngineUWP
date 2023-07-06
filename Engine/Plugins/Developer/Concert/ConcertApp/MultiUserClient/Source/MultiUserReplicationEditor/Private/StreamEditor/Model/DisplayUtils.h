// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

class FString;
class FText;
class UObject;

struct FConcertPropertyChain;
struct FSlateIcon;
struct FSoftObjectPath;

namespace UE::MultiUserReplicationEditor
{
	class IObjectToPropertiesModel;
}

namespace UE::MultiUserReplicationEditor::DisplayUtils
{
	/** @return The text to use for displaying this object's name */
	FText GetObjectDisplayText(const FSoftObjectPath& Object);
	/** @return More lightweight version of GetReplicatedObjectDisplayName which does not construct any FText. */
	FString GetObjectDisplayString(UObject& Object);
	
	/** @return The text to use for displaying this object's type */
	FText GetObjectTypeText(const IObjectToPropertiesModel& Model, const FSoftObjectPath& Object);
	
	/** @return The icon to use for this object */
	FSlateIcon GetObjectIcon(const IObjectToPropertiesModel& Model, const FSoftObjectPath& Object);
	/** @return The icon to use for this object */
	FSlateIcon GetObjectIcon(UObject& Object);

	/** @return The text to use for displaying this property's name. */
	FText GetPropertyDisplayText(const FConcertPropertyChain& Property);
	/** @return More lightweight version of GetPropertyDisplayString which does not construct any FText. */
	FString GetPropertyDisplayString(const FConcertPropertyChain& Property);
}
