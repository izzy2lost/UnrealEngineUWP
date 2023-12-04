// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IStreamExtender.h"
#include "Misc/Attribute.h"

class UObject;
struct FConcertReplicationEditorSettings;

namespace UE::ConcertClientSharedSlate
{
	/** Automatically adds objects and properties according to settings specified in some FConcertReplicationEditorSettings. */
	class CONCERTCLIENTSHAREDSLATE_API FStreamExtenderBySettings : public IStreamExtender
	{
	public:
		
		FStreamExtenderBySettings(TAttribute<const FConcertReplicationEditorSettings*> InReplicationSettingsAttribute);

		//~ Begin IStreamExtender Interface
		virtual void ExtendObjectProperties(UObject& Object, FEnumerateProperties ForEachPropertyToAdd) override;
		virtual void AppendAdditionalObjects(UObject& Object, FEnumerateObjects ForEachAdditionalObject) override;
		//~ End IStreamExtender Interface

	private:
		
		/** Optional settings for auto adding common properties and objects. */
		TAttribute<const FConcertReplicationEditorSettings*> ReplicationSettingsAttribute;
	};
}
