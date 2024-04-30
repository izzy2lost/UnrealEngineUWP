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

void UDataflowBaseContent::SetDataflowOwner(const TObjectPtr<UObject>& InOwner)
{
	DataflowOwner = InOwner;
	if (DataflowContext)
	{
		DataflowContext->Owner = InOwner;  
		SetIsDirty(true);
	}
}

TObjectPtr<UObject> UDataflowBaseContent::GetDataflowOwner() const 
{
	if (DataflowContext)
	{
		ensure(DataflowContext->Owner == DataflowOwner);
	}
	return DataflowContext ? DataflowContext->Owner : DataflowOwner; 
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


void UDataflowBaseContent::BuildBaseContent(TObjectPtr<UObject> InDataflowOwner)
{
	DataflowOwner = InDataflowOwner;
	DataflowContext = MakeShared<Dataflow::FEngineContext>(InDataflowOwner, DataflowAsset, FPlatformTime::Cycles64());
	LastModifiedTimestamp = DataflowContext->GetTimestamp();
	SetIsDirty(true);
	MarkPackageDirty();
}

void UDataflowBaseContent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar << LastModifiedTimestamp;

	if (!DataflowContext)
	{
		DataflowContext = MakeShared<Dataflow::FEngineContext>(DataflowOwner, DataflowAsset, LastModifiedTimestamp);
	}
	DataflowContext->Serialize(Ar);
}

void UDataflowBaseContent::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UDataflowBaseContent* This = CastChecked<UDataflowBaseContent>(InThis);
	Collector.AddReferencedObject(This->DataflowOwner);
	Collector.AddReferencedObject(This->DataflowAsset);
	Super::AddReferencedObjects(InThis, Collector);
}

//
// UDataflowSkeletalContent
//


UDataflowSkeletalContent::UDataflowSkeletalContent() : Super()
{
}


void UDataflowSkeletalContent::RegisterWorldContent(FPreviewScene* PreviewScene, AActor* RootActor)
{
	Super::RegisterWorldContent(PreviewScene, RootActor);

	SkeletalMeshComponent = NewObject<USkeletalMeshComponent>(RootActor);
	SkeletalMeshComponent->SetDisablePostProcessBlueprint(true);
	
	if(SkeletalMesh)
	{
		SkeletalMeshComponent->SetSkeletalMeshAsset(SkeletalMesh);

		Skeleton = SkeletalMesh->GetSkeleton();
		UpdateAnimationInstance();
	}
	SkeletalMeshComponent->UpdateBounds();
	PreviewScene->AddComponent(SkeletalMeshComponent, SkeletalMeshComponent->GetRelativeTransform());
}

void UDataflowSkeletalContent::UpdateAnimationInstance()
{
	if (AnimationAsset && SkeletalMesh && (AnimationAsset->GetSkeleton() == SkeletalMesh->GetSkeleton()))
	{
		AnimationNodeInstance = NewObject<UAnimSingleNodeInstance>(SkeletalMeshComponent);
		AnimationNodeInstance->SetAnimationAsset(AnimationAsset);

		SkeletalMeshComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		SkeletalMeshComponent->InitAnim(true);
		SkeletalMeshComponent->AnimationData.PopulateFrom(AnimationNodeInstance);
		SkeletalMeshComponent->AnimScriptInstance = AnimationNodeInstance;
		SkeletalMeshComponent->AnimScriptInstance->InitializeAnimation();

#if WITH_EDITOR
		SkeletalMeshComponent->ValidateAnimation();
#endif
	}
	else
	{
		SkeletalMeshComponent->Stop();
		SkeletalMeshComponent->AnimationData = FSingleAnimationPlayData();
		SkeletalMeshComponent->AnimScriptInstance = nullptr;
		AnimationNodeInstance = nullptr;
		AnimationAsset = nullptr;
	}
}
	
void UDataflowSkeletalContent::UnregisterWorldContent(FPreviewScene* PreviewScene)
{
	Super::UnregisterWorldContent(PreviewScene);
	if (SkeletalMeshComponent)
	{
		SkeletalMeshComponent->TransformUpdated.RemoveAll(this);
		PreviewScene->RemoveComponent(SkeletalMeshComponent);
	}
}

void UDataflowSkeletalContent::SetSkeletalMesh(const TObjectPtr<USkeletalMesh>& SkeletalMeshAsset)
{
	SkeletalMesh = SkeletalMeshAsset;
	if(SkeletalMesh && SkeletalMesh->GetSkeleton())
	{
		Skeleton = SkeletalMesh->GetSkeleton();
	}
	if(SkeletalMeshComponent)
	{
 		SkeletalMeshComponent->SetSkeletalMeshAsset(SkeletalMesh);

 		UpdateAnimationInstance();
	}
	SetIsDirty(true);
}

void UDataflowSkeletalContent::SetAnimationAsset(const TObjectPtr<UAnimationAsset>& SkeletalAnimationAsset)
{
	AnimationAsset = SkeletalAnimationAsset;
	if(SkeletalMeshComponent)
	{
 		UpdateAnimationInstance();
	}
	SetIsDirty(true);
}

void UDataflowSkeletalContent::SetSkeleton(const TObjectPtr<USkeleton>& SkeletonAsset)
{
	if(SkeletonAsset)
	{
		Skeleton = SkeletonAsset;
		if(SkeletalMesh && (SkeletalMesh->GetSkeleton() != Skeleton))
		{
			SetSkeletalMesh(nullptr);
		}
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
	
	Collector.AddReferencedObject(SkeletalMeshComponent);
	Collector.AddReferencedObject(AnimationNodeInstance);
}

FVector2f UDataflowSkeletalContent::GetSimulationRange() const
{
	if(AnimationNodeInstance)
	{
		return FVector2f(0.0f, AnimationNodeInstance->GetLength());
	}
	return FVector2f(0.0f, 0.0f);
}

void UDataflowSkeletalContent::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UDataflowSkeletalContent* This = CastChecked<UDataflowSkeletalContent>(InThis);
	Collector.AddReferencedObject(This->SkeletalMesh);
	Collector.AddReferencedObject(This->AnimationAsset);
	Collector.AddReferencedObject(This->Skeleton);
	Collector.AddReferencedObject(This->SkeletalMeshComponent);
	Collector.AddReferencedObject(This->AnimationNodeInstance);
	Super::AddReferencedObjects(InThis, Collector);
}
