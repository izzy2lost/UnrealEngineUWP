// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

namespace UE::MultiUserClient
{
	enum class EChangeRevertability
	{
		/** Can click the revert button */
		Revertable,

		/** There are no changes to upload */
		NoChanges,
		/** An upload operation is in progress */
		UploadInProgress,
	};
}