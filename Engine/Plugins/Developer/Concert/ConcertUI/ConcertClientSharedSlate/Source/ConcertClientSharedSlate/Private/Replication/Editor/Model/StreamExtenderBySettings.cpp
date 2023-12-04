// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/Editor/Model/StreamExtenderBySettings.h"

#include "Replication/Data/ConcertPropertySelection.h"
#include "Replication/Settings/ConcertReplicationEditorSettings.h"

namespace UE::ConcertClientSharedSlate
{
	FStreamExtenderBySettings::FStreamExtenderBySettings(TAttribute<const FConcertReplicationEditorSettings*> InReplicationSettingsAttribute)
		: ReplicationSettingsAttribute(MoveTemp(InReplicationSettingsAttribute))
	{}

	void FStreamExtenderBySettings::ExtendObjectProperties(UObject& Object, FEnumerateProperties ForEachPropertyToAdd)
	{
		const FConcertReplicationEditorSettings* Settings = ReplicationSettingsAttribute.Get();
		if (!ensure(Settings))
		{
			return;
		}
		
		Settings->AddDefaultPropertiesFromSettings(*Object.GetClass(), [&ForEachPropertyToAdd](FConcertPropertyChain&& PropertyChain)
		{
			ForEachPropertyToAdd(MoveTemp(PropertyChain));
		});
	}

	void FStreamExtenderBySettings::AppendAdditionalObjects(UObject& Object, FEnumerateObjects ForEachAdditionalObject)
	{
		const FConcertReplicationEditorSettings* Settings = ReplicationSettingsAttribute.Get();
		if (!ensure(Settings))
		{
			return;
		}
		
		Settings->AddAdditionalObjectsFromSettings(Object, [&ForEachAdditionalObject](UObject& AdditionalObject)
		{
			ForEachAdditionalObject(AdditionalObject);
		});
	}
}
