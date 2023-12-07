// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultiUserStreamExtender.h"

#include "MultiUserReplicationSettings.h"

namespace UE::MultiUserClient
{
	FMultiUserStreamExtender::FMultiUserStreamExtender()
		: ExtendBySettings(
			// Use MU settings for auto adding properties & objects
			TAttribute<const FConcertReplicationEditorSettings*>::CreateLambda([]()
			{
				return &UMultiUserReplicationSettings::Get()->ReplicationEditorSettings;
			}))
	{}

	void FMultiUserStreamExtender::ExtendStream(UObject& ExtendedObject, ConcertClientSharedSlate::IStreamExtensionContext& Context)
	{
		ExtendBySettings.ExtendStream(ExtendedObject, Context);
	}
}
