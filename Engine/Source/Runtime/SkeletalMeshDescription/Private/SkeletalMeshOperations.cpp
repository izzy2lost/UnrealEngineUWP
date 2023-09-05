// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkeletalMeshOperations.h"

#include "SkeletalMeshAttributes.h"
#include "BoneWeights.h"


DEFINE_LOG_CATEGORY(LogSkeletalMeshOperations);

#define LOCTEXT_NAMESPACE "SkeletalMeshOperations"

namespace UE::Private
{
	template<typename T>
	struct FCreateAndCopyAttributeValues
	{
		FCreateAndCopyAttributeValues(
			const FMeshDescription& InSourceMesh,
			FMeshDescription& InTargetMesh,
			TArray<FName>& InTargetCustomAttributeNames,
			int32 InTargetVertexIndexOffset)
			: SourceMesh(InSourceMesh)
			, TargetMesh(InTargetMesh)
			, TargetCustomAttributeNames(InTargetCustomAttributeNames)
			, TargetVertexIndexOffset(InTargetVertexIndexOffset)
			{}

		void operator()(const FName InAttributeName, TVertexAttributesConstRef<T> InSrcAttribute)
		{
			// Ignore attributes with reserved names.
			if (FSkeletalMeshAttributes::IsReservedAttributeName(InAttributeName))
			{
				return;
			}

			const bool bAppend = TargetCustomAttributeNames.Contains(InAttributeName);
			if (!bAppend)
			{
				TargetMesh.VertexAttributes().RegisterAttribute<T>(InAttributeName, InSrcAttribute.GetNumChannels(), InSrcAttribute.GetDefaultValue(), InSrcAttribute.GetFlags());
				TargetCustomAttributeNames.Add(InAttributeName);
			}
			//Copy the data
			TVertexAttributesRef<T> TargetVertexAttributes = TargetMesh.VertexAttributes().GetAttributesRef<T>(InAttributeName);
			for (const FVertexID SourceVertexID : SourceMesh.Vertices().GetElementIDs())
			{
				const FVertexID TargetVertexID = FVertexID(TargetVertexIndexOffset + SourceVertexID.GetValue());
				TargetVertexAttributes.Set(TargetVertexID, InSrcAttribute.Get(SourceVertexID));
			}
		}

		// Unhandled sub-types.
		void operator()(const FName, TVertexAttributesConstRef<TArrayAttribute<T>>) { }
		void operator()(const FName, TVertexAttributesConstRef<TArrayView<T>>) { }

	private:
		const FMeshDescription& SourceMesh;
		FMeshDescription& TargetMesh;
		TArray<FName>& TargetCustomAttributeNames;
		int32 TargetVertexIndexOffset = 0;
	};

} // ns UE::Private

//Add specific skeletal mesh descriptions implementation here
void FSkeletalMeshOperations::AppendSkinWeight(const FMeshDescription& SourceMesh, FMeshDescription& TargetMesh, FSkeletalMeshAppendSettings& AppendSettings)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("FSkeletalMeshOperations::AppendSkinWeight");
	FSkeletalMeshConstAttributes SourceSkeletalMeshAttributes(SourceMesh);
	
	FSkeletalMeshAttributes TargetSkeletalMeshAttributes(TargetMesh);
	constexpr bool bKeepExistingAttribute = true;
	TargetSkeletalMeshAttributes.Register(bKeepExistingAttribute);
	
	FSkinWeightsVertexAttributesConstRef SourceVertexSkinWeights = SourceSkeletalMeshAttributes.GetVertexSkinWeights();
	FSkinWeightsVertexAttributesRef TargetVertexSkinWeights = TargetSkeletalMeshAttributes.GetVertexSkinWeights();

	TargetMesh.SuspendVertexIndexing();
	
	//Append Custom VertexAttribute
	if(AppendSettings.bAppendVertexAttributes)
	{
		TArray<FName> TargetCustomAttributeNames;
		TargetMesh.VertexAttributes().GetAttributeNames(TargetCustomAttributeNames);
		int32 TargetVertexIndexOffset = FMath::Max(TargetMesh.Vertices().Num() - SourceMesh.Vertices().Num(), 0);

		SourceMesh.VertexAttributes().ForEachByType<float>(UE::Private::FCreateAndCopyAttributeValues<float>(SourceMesh, TargetMesh, TargetCustomAttributeNames, AppendSettings.SourceVertexIDOffset));
		SourceMesh.VertexAttributes().ForEachByType<FVector2f>(UE::Private::FCreateAndCopyAttributeValues<FVector2f>(SourceMesh, TargetMesh, TargetCustomAttributeNames, AppendSettings.SourceVertexIDOffset));
		SourceMesh.VertexAttributes().ForEachByType<FVector3f>(UE::Private::FCreateAndCopyAttributeValues<FVector3f>(SourceMesh, TargetMesh, TargetCustomAttributeNames, AppendSettings.SourceVertexIDOffset));
		SourceMesh.VertexAttributes().ForEachByType<FVector4f>(UE::Private::FCreateAndCopyAttributeValues<FVector4f>(SourceMesh, TargetMesh, TargetCustomAttributeNames, AppendSettings.SourceVertexIDOffset));
	}

	for (const FVertexID SourceVertexID : SourceMesh.Vertices().GetElementIDs())
	{
		const FVertexID TargetVertexID = FVertexID(AppendSettings.SourceVertexIDOffset + SourceVertexID.GetValue());
		FVertexBoneWeightsConst SourceBoneWeights = SourceVertexSkinWeights.Get(SourceVertexID);
		TArray<UE::AnimationCore::FBoneWeight> TargetBoneWeights;
		const int32 InfluenceCount = SourceBoneWeights.Num();
		for (int32 InfluenceIndex = 0; InfluenceIndex < InfluenceCount; ++InfluenceIndex)
		{
			const FBoneIndexType SourceBoneIndex = SourceBoneWeights[InfluenceIndex].GetBoneIndex();
			if(AppendSettings.SourceRemapBoneIndex.IsValidIndex(SourceBoneIndex))
			{
				UE::AnimationCore::FBoneWeight& TargetBoneWeight = TargetBoneWeights.AddDefaulted_GetRef();
				TargetBoneWeight.SetBoneIndex(AppendSettings.SourceRemapBoneIndex[SourceBoneIndex]);
				TargetBoneWeight.SetRawWeight(SourceBoneWeights[InfluenceIndex].GetRawWeight());
			}
		}
		TargetVertexSkinWeights.Set(TargetVertexID, TargetBoneWeights);
	}

	TargetMesh.ResumeVertexIndexing();
}

#undef LOCTEXT_NAMESPACE