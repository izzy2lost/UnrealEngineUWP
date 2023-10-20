// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Editor/Model/ReplicatedObjectData.h"

namespace UE::ConcertClientSharedSlate
{
	/** Type of items in SSubobjectView's tree view */
	class FReplicatedSubobjectData
	{
	public:

		FReplicatedSubobjectData() = default;
		explicit FReplicatedSubobjectData(FSoftObjectPath ObjectPath)
			: bIsSeparator(false)
			, ObjectData(MoveTemp(ObjectPath))
		{}

		bool IsSeparator() const { return bIsSeparator; }
		const TOptional<FReplicatedObjectData>& GetObjectData() const { return ObjectData; }
		
	private:

		/** Whether this is a separator (i.e. whether it separates categories). */
		bool bIsSeparator = true;
		/** Set if bIsSeparator == false. */
		TOptional<FReplicatedObjectData> ObjectData;
	};
}