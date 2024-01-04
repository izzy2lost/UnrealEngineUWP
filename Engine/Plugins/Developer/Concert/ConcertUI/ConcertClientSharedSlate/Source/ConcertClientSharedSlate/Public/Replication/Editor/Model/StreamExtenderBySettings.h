// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Extension/IStreamExtender.h"
#include "Misc/Attribute.h"

class UObject;
struct FConcertStreamObjectAutoBindingRules;

namespace UE::ConcertClientSharedSlate
{
	/** Automatically adds objects and properties according to settings specified in some FConcertReplicationEditorSettings. */
	class CONCERTCLIENTSHAREDSLATE_API FStreamExtenderBySettings : public IStreamExtender
	{
	public:
		
		FStreamExtenderBySettings(TAttribute<const FConcertStreamObjectAutoBindingRules*> InReplicationSettingsAttribute);

		//~ Begin IStreamExtender Interface
		virtual void ExtendStream(UObject& ExtendedObject, IStreamExtensionContext& Context) override;
		//~ End IStreamExtender Interface

	private:
		
		/** Optional settings for auto adding common properties and objects. */
		TAttribute<const FConcertStreamObjectAutoBindingRules*> ReplicationSettingsAttribute;
	};
}
