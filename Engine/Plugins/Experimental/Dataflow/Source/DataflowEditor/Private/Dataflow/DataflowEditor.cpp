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

UDataflowEditor::UDataflowEditor() : Super()
{
	Content = NewObject<UDataflowEditorContent>(this, MakeUniqueObjectName(this, UDataflowEditor::StaticClass(), "EditorData"));
}

TSharedPtr<FBaseAssetToolkit> UDataflowEditor::CreateToolkit()
{
	TSharedPtr<FDataflowEditorToolkit> DataflowToolkit = MakeShared<FDataflowEditorToolkit>(this);
	return DataflowToolkit;
}

void UDataflowEditor::Initialize(const TArray<TObjectPtr<UObject>>& InObjects)
{
	check(Content);

	TArray<TObjectPtr<UObject>> ObjectsToEdit;
	Content->bHasValidSkeletalMesh = false;
	if (ensure(InObjects.Num() == 1))
	{
		TObjectPtr<UObject> RootObject = InObjects[0];
		Content->DataflowAsset = Cast<UDataflow>(RootObject);
		if(!Content->DataflowAsset)
		{
			Content->DataflowAsset = Private::GetDataflowAssetFrom(RootObject);
			Content->DataflowTerminal = Private::GetDataflowTerminalFrom(RootObject);
			Content->SkeletalMesh = Private::GetSkeletalMeshFrom(RootObject);
			Content->AnimationAsset = Private::GetAnimationAssetFrom(RootObject);
		}
		if(Content->DataflowAsset)
		{
			Content->DataflowOwner = RootObject;
			Content->DataflowAsset->Schema = UDataflowSchema::StaticClass();
			Content->DataflowContext = MakeShared<Dataflow::FEngineContext>(RootObject, Content->DataflowAsset, FPlatformTime::Cycles64());
			Content->LastNodeTimestamp = Content->DataflowContext->GetTimestamp();

			ObjectsToEdit.Add(Content->DataflowOwner);

			if(!Content->SkeletalMesh)
			{
				const FName SkeletonName = MakeUniqueObjectName(Content->DataflowAsset, UDataflow::StaticClass(), FName("USkeleton"));
				const FName SkeletalMeshName = MakeUniqueObjectName(Content->DataflowAsset, UDataflow::StaticClass(), FName("USkeletalMesh"));

				Content->Skeleton = NewObject<USkeleton>(Content->DataflowAsset, SkeletonName);
				Content->SkeletalMesh = NewObject<USkeletalMesh>(Content->DataflowAsset, SkeletalMeshName);
				Content->SkeletalMesh->SetSkeleton(Content->Skeleton);
			}
			else
			{
				Content->Skeleton = Content->SkeletalMesh->GetSkeleton();
				Content->bHasValidSkeletalMesh = true;
			}
		}
	}
	// Potentially we could add additional objects to edit here (fields, meshes....)
	// If these objects have a matching factory we would be able to use geometry tools
	UBaseCharacterFXEditor::Initialize(ObjectsToEdit);
}



