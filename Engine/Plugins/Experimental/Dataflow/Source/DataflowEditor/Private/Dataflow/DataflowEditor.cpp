// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowEditor.h"

#include "Animation/Skeleton.h"
#include "Dataflow/DataflowEditorToolkit.h"
#include "Engine/SkeletalMesh.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DataflowEditor)

DEFINE_LOG_CATEGORY(LogDataflowEditor);

namespace Private
{
	UDataflow* GetDataflowAssetFrom(UObject* InObject)
	{
		if (UClass* Class = InObject->GetClass())
		{
			if (FProperty* Property = Class->FindPropertyByName(FName("DataflowAsset")))
			{
				return *Property->ContainerPtrToValuePtr<UDataflow*>(InObject);
			}
		}
		return nullptr;
	}

	USkeletalMesh* GetSkeletalMeshFrom(UObject* InObject)
	{
		if (UClass* Class = InObject->GetClass())
		{
			if (FProperty* Property = Class->FindPropertyByName(FName("SkeletalMesh")))
			{
				return *Property->ContainerPtrToValuePtr<USkeletalMesh*>(InObject);
			}
		}
		return nullptr;
	}
	
	UAnimationAsset* GetAnimationAssetFrom(UObject* InObject)
	{
		if (UClass* Class = InObject->GetClass())
		{
			if (FProperty* Property = Class->FindPropertyByName(FName("AnimationAsset")))
			{
				return *Property->ContainerPtrToValuePtr<UAnimationAsset*>(InObject);
			}
		}
		return nullptr;
	}

	FString GetDataflowTerminalFrom(UObject* InObject)
	{
		if (UClass* Class = InObject->GetClass())
		{
			if (FProperty* Property = Class->FindPropertyByName(FName("DataflowTerminal")))
			{
				return *Property->ContainerPtrToValuePtr<FString>(InObject);
			}
		}
		return FString();
	}
};

TSharedPtr<FBaseAssetToolkit> UDataflowEditor::CreateToolkit()
{
	TSharedPtr<FDataflowEditorToolkit> DataflowToolkit = MakeShared<FDataflowEditorToolkit>(this);
	return DataflowToolkit;
}

void UDataflowEditor::Initialize(const TArray<TObjectPtr<UObject>>& InObjects)
{
	// We extract the dataflow and the associated datas from the list of objects
	TArray<TObjectPtr<UObject>> ObjectsToEdit;
	DataflowDatas.bHasValidSkeletalMesh = false;
	if (ensure(InObjects.Num() == 1))
	{
		TObjectPtr<UObject> RootObject = InObjects[0];
		DataflowDatas.DataflowAsset = Cast<UDataflow>(RootObject);
		if(!DataflowDatas.DataflowAsset)
		{
			DataflowDatas.DataflowAsset = Private::GetDataflowAssetFrom(RootObject);
			DataflowDatas.DataflowTerminal = Private::GetDataflowTerminalFrom(RootObject);
			DataflowDatas.SkeletalMesh = Private::GetSkeletalMeshFrom(RootObject);
			DataflowDatas.AnimationAsset = Private::GetAnimationAssetFrom(RootObject);
		}
		if(DataflowDatas.DataflowAsset)
		{
			DataflowDatas.DataflowOwner = RootObject;
			DataflowDatas.DataflowAsset->Schema = UDataflowSchema::StaticClass();
			DataflowDatas.DataflowContext = MakeShared<Dataflow::FEngineContext>(RootObject, DataflowDatas.DataflowAsset, FPlatformTime::Cycles64());
			DataflowDatas.LastNodeTimestamp = DataflowDatas.DataflowContext->GetTimestamp();

			ObjectsToEdit.Add(DataflowDatas.DataflowOwner);

			if(!DataflowDatas.SkeletalMesh)
			{
				const FName SkeletonName = MakeUniqueObjectName(DataflowDatas.DataflowAsset, UDataflow::StaticClass(), FName("USkeleton"));
				const FName SkeletalMeshName = MakeUniqueObjectName(DataflowDatas.DataflowAsset, UDataflow::StaticClass(), FName("USkeletalMesh"));

				DataflowDatas.Skeleton = NewObject<USkeleton>(DataflowDatas.DataflowAsset, SkeletonName);
				DataflowDatas.SkeletalMesh = NewObject<USkeletalMesh>(DataflowDatas.DataflowAsset, SkeletalMeshName);
				DataflowDatas.SkeletalMesh->SetSkeleton(DataflowDatas.Skeleton);
			}
			else
			{
				DataflowDatas.Skeleton = DataflowDatas.SkeletalMesh->GetSkeleton();
				DataflowDatas.bHasValidSkeletalMesh = true;
			}
		}
	}
	// Potentially we could add additional objects to edit here (fields, meshes....)
	// If these objects have a matching factory we would be able to use geometry tools
	UBaseCharacterFXEditor::Initialize(ObjectsToEdit);
}



