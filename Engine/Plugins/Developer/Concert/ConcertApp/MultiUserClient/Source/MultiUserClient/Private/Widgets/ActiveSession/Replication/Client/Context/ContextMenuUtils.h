// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "UObject/SoftObjectPath.h"

class IConcertClient;
class FMenuBuilder;

namespace UE::ConcertSharedSlate
{
	class IMultiReplicationStreamEditor;
	class IObjectHierarchyModel;
}
namespace UE::MultiUserClient
{
	class FReassignObjectPropertiesLogic;
	class FReplicationClientManager;
}

namespace UE::MultiUserClient::ContextMenuUtils
{
	/** Adds menu entries for reassigning the object to another client. */
	void AddReassignmentOptions(
		FMenuBuilder& MenuBuilder,
		const FSoftObjectPath& ContextObject,
		const IConcertClient& ConcertClient,
		const FReplicationClientManager& ReplicationManager,
		ConcertSharedSlate::IObjectHierarchyModel& ObjectHierarchy,
		FReassignObjectPropertiesLogic& ReassignmentLogic,
		ConcertSharedSlate::IMultiReplicationStreamEditor& MultiStreamEditor
		);
	
	/** Adds an edit box for batch reassigning the select object's frequencies for all replicating clients. */
	void AddFrequencyOptionsForMultipleClients(FMenuBuilder& MenuBuilder, const FSoftObjectPath& ContextObjects, FReplicationClientManager& InClientManager);
	/** Adds an edit box for batch reassigning the select object's frequencies for all replicating clients. */
	inline void AddFrequencyOptionsIfOneContextObject_MultiClient(FMenuBuilder& MenuBuilder, TConstArrayView<FSoftObjectPath> ContextObjects, FReplicationClientManager& InClientManager)
	{
		if (ContextObjects.Num() == 1)
		{
			AddFrequencyOptionsForMultipleClients(MenuBuilder, ContextObjects[0], InClientManager);
		}
	}
}