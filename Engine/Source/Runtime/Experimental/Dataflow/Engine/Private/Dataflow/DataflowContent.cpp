// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowContent.h"

#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dataflow/DataflowNodeParameters.h"
#include "Dataflow/DataflowObject.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"
#include "PreviewScene.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DataflowContent)

void UDataflowContextObject::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UDataflowContextObject* This = CastChecked<UDataflowContextObject>(InThis);
	Collector.AddReferencedObject(This->PrimarySelectedNode);
	Super::AddReferencedObjects(InThis, Collector);
}

TObjectPtr<UDataflowBaseContent> IDataflowContentOwner::BuildDataflowContent()
{
	if(TObjectPtr<UDataflowBaseContent> DataflowContent = CreateDataflowContent())
	{
		// Delegate used for notifying owner data invalidation
		OnContentOwnerChanged.AddUObject(DataflowContent, &UDataflowBaseContent::UpdateContentDatas);
		return DataflowContent;
	}
	return nullptr;
}

//
// UDataflowBaseContent
//

void UDataflowBaseContent::SetIsDirty(bool InDirty) 
{ 
	bIsDirty = InDirty;
}

UDataflowBaseContent::UDataflowBaseContent()
{
}

UDataflowBaseContent::~UDataflowBaseContent()
{
	if(GetDataflowOwner())
	{
		if(IDataflowContentOwner* ContentOwner = Cast<IDataflowContentOwner>(GetDataflowOwner()))
		{
			ContentOwner->OnContentOwnerChanged.RemoveAll(this);
		}
	}
}

void UDataflowBaseContent::UpdateContentDatas()
{
	if(GetDataflowOwner())
	{
		if(const IDataflowContentOwner* ContentOwner = Cast<IDataflowContentOwner>(GetDataflowOwner()))
		{
			ContentOwner->UpdateDataflowContent(this);
		}
	}
}

void UDataflowBaseContent::SetDataflowOwner(const TObjectPtr<UObject>& InOwner)
{
	if(!DataflowContext)
	{
		DataflowContext = MakeShared<Dataflow::FEngineContext>(nullptr, nullptr, Dataflow::FTimestamp::Invalid);
	}
	DataflowContext->Owner = InOwner;  
	SetIsDirty(true);
}

TObjectPtr<UObject> UDataflowBaseContent::GetDataflowOwner() const 
{
	return DataflowContext ? DataflowContext->Owner : nullptr; 
}

void UDataflowBaseContent::SetDataflowAsset(const TObjectPtr<UDataflow>& DataflowAsset)
{
	if(!DataflowContext)
	{
		DataflowContext = MakeShared<Dataflow::FEngineContext>(nullptr, nullptr, Dataflow::FTimestamp::Invalid);
	}
	DataflowContext->Graph = DataflowAsset;  
	SetIsDirty(true);
}

TObjectPtr<UDataflow> UDataflowBaseContent::GetDataflowAsset() const 
{
	return DataflowContext ? DataflowContext->Graph : nullptr; 
}

void UDataflowBaseContent::SetLastModifiedTimestamp(Dataflow::FTimestamp InTimestamp, bool bMakeDirty) 
{ 
	if (InTimestamp.IsInvalid() || LastModifiedTimestamp < InTimestamp)
	{
		LastModifiedTimestamp = InTimestamp; 
		if (bMakeDirty)
		{
			SetIsDirty(true);
			MarkPackageDirty(); 
		}
	}
}

void UDataflowBaseContent::SetDataflowContext(const TSharedPtr<Dataflow::FEngineContext>& InContext) 
{ 
	DataflowContext = InContext;  
	SetIsDirty(true); 
	MarkPackageDirty();
}

void UDataflowBaseContent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar << LastModifiedTimestamp;

	if (!DataflowContext)
	{
		DataflowContext = MakeShared<Dataflow::FEngineContext>(nullptr, nullptr, LastModifiedTimestamp);
	}
	DataflowContext->Serialize(Ar);
}

void UDataflowBaseContent::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UDataflowBaseContent* This = CastChecked<UDataflowBaseContent>(InThis);
	if(This->DataflowContext)
	{
		Collector.AddReferencedObject(This->DataflowContext->Owner);
		Collector.AddReferencedObject(This->DataflowContext->Graph);
	}
	Super::AddReferencedObjects(InThis, Collector);
}

//
// UDataflowSkeletalContent
//

UDataflowSkeletalContent::UDataflowSkeletalContent() : Super()
{
}

void UDataflowSkeletalContent::SetSkeletalMesh(const TObjectPtr<USkeletalMesh>& SkeletalMeshAsset)
{
	SkeletalMesh = SkeletalMeshAsset;
	if(SkeletalMesh)
	{
		if(SkeletalMesh && (SkeletalMesh->GetSkeleton() != Skeleton))
		{
			SetSkeleton(SkeletalMesh->GetSkeleton());
		}
	}
	SetIsDirty(true);
}

void UDataflowSkeletalContent::SetAnimationAsset(const TObjectPtr<UAnimationAsset>& SkeletalAnimationAsset)
{
	AnimationAsset = SkeletalAnimationAsset;
	if(AnimationAsset && (AnimationAsset->GetSkeleton()) != Skeleton)
	{
		SetSkeleton(AnimationAsset->GetSkeleton());
	}
	SetIsDirty(true);
}

void UDataflowSkeletalContent::SetSkeleton(const TObjectPtr<USkeleton>& SkeletonAsset)
{
	Skeleton = SkeletonAsset;
	if(SkeletalMesh && (SkeletalMesh->GetSkeleton() != Skeleton))
	{
		SetSkeletalMesh(nullptr);
	}
	if(AnimationAsset && (AnimationAsset->GetSkeleton() != Skeleton))
	{
		SetAnimationAsset(nullptr);
	}
	SetIsDirty(true);
}

#if WITH_EDITOR

void UDataflowSkeletalContent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) 
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	const FName PropertyName = PropertyChangedEvent.Property->GetFName();
	
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UDataflowSkeletalContent, SkeletalMesh))
	{
		SetSkeletalMesh(SkeletalMesh);
	}
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UDataflowSkeletalContent, AnimationAsset))
	{
		SetAnimationAsset(AnimationAsset);
	}
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UDataflowSkeletalContent, Skeleton))
	{
		SetSkeleton(Skeleton);
	}
}

#endif //if WITH_EDITOR

void UDataflowSkeletalContent::AddContentObjects(FReferenceCollector& Collector)
{
	Super::AddContentObjects(Collector);
}

void UDataflowSkeletalContent::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UDataflowSkeletalContent* This = CastChecked<UDataflowSkeletalContent>(InThis);
	Collector.AddReferencedObject(This->SkeletalMesh);
	Collector.AddReferencedObject(This->AnimationAsset);
	Collector.AddReferencedObject(This->Skeleton);
	Super::AddReferencedObjects(InThis, Collector);
}

