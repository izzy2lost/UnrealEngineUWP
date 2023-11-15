// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

namespace UE::MultiUserClient
{
	enum class EChangeUploadability
	{
		/** Can click the upload button */
		Ready,

		/** There are no changes to upload */
		NoChanges,
		/** A previous upload operation is in progress */
		InProgress,
		
		// TODO DP UE-197435: Remove once remote client streams can be changed
		// TODO DP UE-198088: Remove once remote changing is implemented
		NotImplemented
	};

	bool CanEverSubmit(EChangeUploadability Uploadability)
	{
		return Uploadability != EChangeUploadability::NotImplemented;
	}
}