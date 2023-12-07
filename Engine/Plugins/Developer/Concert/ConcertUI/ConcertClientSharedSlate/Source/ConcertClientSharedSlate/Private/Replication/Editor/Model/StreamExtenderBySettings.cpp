// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/Editor/Model/StreamExtenderBySettings.h"

#include "Replication/Data/ConcertPropertySelection.h"
#include "Replication/Editor/Model/Extension/IStreamExtensionContext.h"
#include "Replication/Settings/ConcertReplicationEditorSettings.h"

namespace UE::ConcertClientSharedSlate
{
	FStreamExtenderBySettings::FStreamExtenderBySettings(TAttribute<const FConcertReplicationEditorSettings*> InReplicationSettingsAttribute)
		: ReplicationSettingsAttribute(MoveTemp(InReplicationSettingsAttribute))
	{}

	void FStreamExtenderBySettings::ExtendStream(UObject& ExtendedObject, IStreamExtensionContext& Context)
	{
		const FConcertReplicationEditorSettings* Settings = ReplicationSettingsAttribute.Get();
		if (!ensure(Settings))
		{
			return;
		}
		Settings->AddDefaultPropertiesFromSettings(*ExtendedObject.GetClass(), [&ExtendedObject, &Context](FConcertPropertyChain&& PropertyChain)
		{
			Context.AddPropertyTo(ExtendedObject, MoveTemp(PropertyChain));
		});
		Settings->AddAdditionalObjectsFromSettings(ExtendedObject, [&ExtendedObject, &Context](UObject& AdditionalObject)
		{
			Context.AddAdditionalObject(AdditionalObject);
		});
	}
}
