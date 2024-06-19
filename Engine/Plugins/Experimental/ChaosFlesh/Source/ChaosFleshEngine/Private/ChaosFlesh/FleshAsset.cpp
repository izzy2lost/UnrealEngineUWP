// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	FleshAsset.cpp: UFleshAsset methods.
=============================================================================*/
#include "ChaosFlesh/FleshAsset.h"
#include "ChaosFlesh/FleshCollection.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dataflow/DataflowContent.h"
#include "Engine/SkeletalMesh.h"
#include "GeometryCollection/TransformCollection.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FleshAsset)

DEFINE_LOG_CATEGORY_STATIC(LogFleshAssetInternal, Log, All);


FFleshAssetEdit::FFleshAssetEdit(UFleshAsset* InAsset, FPostEditFunctionCallback InCallback)
	: PostEditCallback(InCallback)
	, Asset(InAsset)
{
}

FFleshAssetEdit::~FFleshAssetEdit()
{
	PostEditCallback();
}

FFleshCollection* FFleshAssetEdit::GetFleshCollection()
{
	if (Asset)
	{
		return Asset->FleshCollection.Get();
	}
	return nullptr;
}

UFleshAsset::UFleshAsset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, FleshCollection(new FFleshCollection())
{
}

void UFleshAsset::SetCollection(FFleshCollection* InCollection)
{
	FleshCollection = TSharedPtr<FFleshCollection, ESPMode::ThreadSafe>(InCollection);
	Modify();
}


void UFleshAsset::PostEditCallback()
{
	//UE_LOG(LogFleshAssetInternal, Log, TEXT("UFleshAsset::PostEditCallback()"));
}

TManagedArray<FVector3f>& UFleshAsset::GetPositions()
{
	return FleshCollection->ModifyAttribute<FVector3f>("Vertex", FGeometryCollection::VerticesGroup);
}

const TManagedArray<FVector3f>* UFleshAsset::FindPositions() const
{
	return FleshCollection->FindAttributeTyped<FVector3f>("Vertex", FGeometryCollection::VerticesGroup);
}

/** Serialize */
void UFleshAsset::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	bool bCreateSimulationData = false;
	Chaos::FChaosArchive ChaosAr(Ar);
	FleshCollection->Serialize(ChaosAr);
}

TObjectPtr<UDataflowBaseContent> UFleshAsset::CreateDataflowContent()
{
	TObjectPtr<UDataflowSkeletalContent> SkeletalContent = DataflowContextHelpers::CreateNewDataflowContent<UDataflowSkeletalContent>(this);

	SkeletalContent->SetDataflowOwner(this);
	SkeletalContent->SetTerminalAsset(this);

	WriteDataflowContent(SkeletalContent);
	
	return SkeletalContent;
}

void UFleshAsset::WriteDataflowContent(const TObjectPtr<UDataflowBaseContent>& DataflowContent) const
{
	if(const TObjectPtr<UDataflowSkeletalContent> SkeletalContent = Cast<UDataflowSkeletalContent>(DataflowContent))
	{
		SkeletalContent->SetDataflowAsset(DataflowAsset);
		SkeletalContent->SetDataflowTerminal(DataflowTerminal);
		
		SkeletalContent->SetSkeletalMesh(SkeletalMesh, true);
		SkeletalContent->SetSkeleton(Skeleton);

#if WITH_EDITORONLY_DATA
		SkeletalContent->SetAnimationAsset(PreviewAnimationAsset.Get());
#endif
	}
}

void UFleshAsset::ReadDataflowContent(const TObjectPtr<UDataflowBaseContent>& DataflowContent)
{
	if(const TObjectPtr<UDataflowSkeletalContent> SkeletalContent = Cast<UDataflowSkeletalContent>(DataflowContent))
	{
#if WITH_EDITORONLY_DATA
		PreviewAnimationAsset = SkeletalContent->GetAnimationAsset();
#endif
	}
}

#if WITH_EDITOR
void UFleshAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.Property->GetFName();
    
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UFleshAsset, SkeletalMesh))
	{
		if(SkeletalMesh && (SkeletalMesh->GetSkeleton() != Skeleton))
		{
			Skeleton = SkeletalMesh->GetSkeleton();
		}
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UFleshAsset, Skeleton))
	{
		if(SkeletalMesh && (SkeletalMesh->GetSkeleton() != Skeleton))
		{
			SkeletalMesh = nullptr;
		}
	}
	InvalidateDataflowContents();
}
#endif //if WITH_EDITOR


