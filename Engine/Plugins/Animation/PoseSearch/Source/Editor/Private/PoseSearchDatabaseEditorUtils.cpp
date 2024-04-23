// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchDatabaseEditorUtils.h"

#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchSchema.h"

namespace UE::PoseSearch
{
	bool FPoseSearchEditorUtils::IsAssetCompatibleWithDatabase(const UPoseSearchDatabase* InDatabase, const FAssetData& InAssetData)
	{
		if (InDatabase)
		{
			if (InDatabase->Schema)
			{
				TArray<FPoseSearchRoledSkeleton> RoledSkeletons = InDatabase->Schema->GetRoledSkeletons();

				for (const FPoseSearchRoledSkeleton& RoledSkeleton : RoledSkeletons)
				{
					if (RoledSkeleton.Skeleton && RoledSkeleton.Skeleton->IsCompatibleForEditor(InAssetData))
					{
						// We found a compatible skeleton in the schema.
						return true;
					}
				}
			}
		}
		
		return false;
	}
}
