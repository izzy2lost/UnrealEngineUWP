// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/SoftObjectPath.h"

namespace UE::ConcertClientSharedSlate
{
	/** Instanced for each object row in SObjectToPropertyViewer.*/
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

	/** Instanced for each object row SObjectToPropertyEditor. */
	class FReplicatedObjectData_Editor : public FReplicatedObjectData
	{
		// Empty and unused for now. I just want to highlight that if you want custom data for SObjectToPropertyEditor,
		// you should add it here and override SObjectToPropertyViewer::AllocateObjectData.
	};
}