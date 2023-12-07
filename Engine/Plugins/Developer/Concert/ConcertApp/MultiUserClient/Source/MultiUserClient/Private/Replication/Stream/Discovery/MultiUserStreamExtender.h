// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Editor/Model/Extension/IStreamExtender.h"
#include "Replication/Editor/Model/StreamExtenderBySettings.h"

namespace UE::MultiUserClient
{
	class FReplicationDiscoveryContainer;
	
	/**
	 * When the user adds an object, this object handles auto selecting properties and adding additional objects from context.
	 * The following sources exist:
	 * - Static Settings: user can specify properties & objects in the MU project settings
	 * - Dynamic API: by using the IMultiUserReplication, external modules can register dynamic rules for auto-discovery.
	 */
	class FMultiUserStreamExtender : public ConcertClientSharedSlate::IStreamExtender
	{
	public:

		FMultiUserStreamExtender();

		//~ Begin IStreamExtender Interface
		virtual void ExtendStream(UObject& ExtendedObject, ConcertClientSharedSlate::IStreamExtensionContext& Context) override;
		//~ End IStreamExtender Interface

	private:

		/** Handles properties from the MU settings. */
		ConcertClientSharedSlate::FStreamExtenderBySettings ExtendBySettings;
	};
}

