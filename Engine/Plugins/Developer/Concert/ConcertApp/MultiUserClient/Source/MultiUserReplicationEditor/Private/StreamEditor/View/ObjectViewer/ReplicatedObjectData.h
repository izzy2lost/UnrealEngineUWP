// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/SoftObjectPath.h"

namespace UE::MultiUserReplicationEditor
{
	/** Instanced for each object row in SPropertyReplicationSelectionViewer.*/
	class FReplicatedObjectData
	{
	public:

		FReplicatedObjectData(FSoftObjectPath ObjectPath)
			: ObjectPath(MoveTemp(ObjectPath))
		{}
		
		const FSoftObjectPath& GetObjectPath() const { return ObjectPath; }
		void SetObjectPath(FSoftObjectPath InObjectPath) { ObjectPath = MoveTemp(InObjectPath); }

	private:

		/** The object identified by the */
		FSoftObjectPath ObjectPath;
	};

	/** Instanced for each object row SPropertyReplicationSelectionEditor. */
	class FReplicatedObjectData_Editor : public FReplicatedObjectData
	{
		// Empty and unused for now. I just want to highlight that if you want custom data for SPropertyReplicationSelectionEditor,
		// you should add it here and override SPropertyReplicationSelectionViewer::AllocateObjectData.
	};
}