// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "Logging/LogMacros.h"

struct FManagedArrayCollection;
struct FDataflowNode;
class FSkeletalMeshLODModel;
class FString;

namespace UE::Chaos::ClothAsset
{
	/**
	* Tools shared by cloth dataflow nodes
	*/
	struct FClothDataflowTools
	{
		static void AddRenderPatternFromSkeletalMeshSection(const TSharedRef<FManagedArrayCollection>& ClothCollection, const FSkeletalMeshLODModel& SkeletalMeshModel, const int32 SectionIndex, const FString& RenderMaterialPathName);

		static void AddSimPatternsFromSkeletalMeshSection(const TSharedRef<FManagedArrayCollection>& ClothCollection, const FSkeletalMeshLODModel& SkeletalMeshModel, const int32 SectionIndex, const int32 UVChannelIndex, const FVector2f& UVScale);

		static void LogAndToastWarning(const FDataflowNode& DataflowNode, const FText& Headline, const FText& Details);

		/**
		 * Turn a string into a valid collection group or attribute name.
		 * The resulting name won't contains spaces and any other special characters as listed in
		 * INVALID_OBJECTNAME_CHARACTERS (currently "',/.:|&!~\n\r\t@#(){}[]=;^%$`).
		 * It will also have all leading underscore removed, as these names are reserved for internal use.
		 */
		static void MakeCollectionName(FString& InOutString);
	};
}  // End namespace UE::Chaos::ClothAsset

DECLARE_LOG_CATEGORY_EXTERN(LogChaosClothAssetDataflowNodes, Log, All);
