// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowEditor.h"

#include "Animation/Skeleton.h"
#include "Dataflow/AssetDefinition_DataflowContext.h"
#include "Dataflow/DataflowContent.h"
#include "Dataflow/DataflowEditorToolkit.h"
#include "Dataflow/DataflowEditorUtil.h"
#include "Engine/SkeletalMesh.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DataflowEditor)

DEFINE_LOG_CATEGORY(LogDataflowEditor);

UDataflowEditor::UDataflowEditor() : Super()
{}

TSharedPtr<FBaseAssetToolkit> UDataflowEditor::CreateToolkit()
{
	TSharedPtr<FDataflowEditorToolkit> DataflowToolkit = MakeShared<FDataflowEditorToolkit>(this);
	return DataflowToolkit;
}

void UDataflowEditor::Initialize(const TArray<TObjectPtr<UObject>>& InObjects)
{
	if(!InObjects.IsEmpty())
	{
		InitializeContent(InObjects[0]);
	}
}

void UDataflowEditor::InitializeContent(const TObjectPtr<UObject>& ContentOwner)
{
	check(DataflowContent == nullptr);

	TArray<TObjectPtr<UObject>> RequiredObjects = { ContentOwner };

	if (UDataflow* DataflowAsset = Cast<UDataflow>(ContentOwner))
	{
		DataflowContent = DataflowContextDefinitionHelpers::CreateNewDataflowContext<UDataflowBaseContent>(ContentOwner);

		DataflowContent->SetDataflowAsset(DataflowAsset);
		DataflowContent->SetDataflowTerminal(FString());
	}
	else
	{
		if (Private::HasDataflowAsset(ContentOwner))
		{
			if (Private::HasSkeletalMesh(ContentOwner))
			{
				DataflowContent = DataflowContextDefinitionHelpers::CreateNewDataflowContext<UDataflowSkeletalContent>(ContentOwner);
				const TObjectPtr<UDataflowSkeletalContent> SkeletalContent = Cast<UDataflowSkeletalContent>(DataflowContent);

				SkeletalContent->SetSkeletalMesh(Private::GetSkeletalMeshFrom(ContentOwner));
				SkeletalContent->SetSkeleton(Private::GetSkeletonFrom(ContentOwner));
				SkeletalContent->SetAnimationAsset(Private::GetAnimationAssetFrom(ContentOwner));
			}
			else
			{
				DataflowContent = DataflowContextDefinitionHelpers::CreateNewDataflowContext<UDataflowBaseContent>(ContentOwner);
			}

			DataflowContent->SetDataflowAsset(Private::GetDataflowAssetFrom(ContentOwner));
			DataflowContent->SetDataflowTerminal(Private::GetDataflowTerminalFrom(ContentOwner));
			RequiredObjects.Add(Private::GetDataflowAssetFrom(ContentOwner));
		}
	}

	if (!DataflowContent) return;
	RequiredObjects.Add(DataflowContent);

	if (const TObjectPtr<UDataflowSkeletalContent> SkeletalContent = Cast<UDataflowSkeletalContent>(DataflowContent))
	{
		if (!SkeletalContent->GetSkeletalMesh())
		{
			const FName SkeletalMeshName = MakeUniqueObjectName(SkeletalContent->GetDataflowAsset(), UDataflow::StaticClass(), FName("USkeletalMesh"));
			USkeletalMesh* SkeletalMesh = NewObject<USkeletalMesh>(SkeletalContent->GetDataflowAsset(), SkeletalMeshName);

			USkeleton* Skeleton = SkeletalContent->GetSkeleton();
			if (!Skeleton)
			{
				const FName SkeletonName = MakeUniqueObjectName(SkeletalContent->GetDataflowAsset(), UDataflow::StaticClass(), FName("USkeleton"));
				Skeleton = NewObject<USkeleton>(SkeletalContent->GetDataflowAsset(), SkeletonName);
			}
			SkeletalMesh->SetSkeleton(Skeleton);
			SkeletalContent->SetSkeletalMesh(SkeletalMesh);
		}
		else if (!SkeletalContent->GetSkeleton())
		{
			SkeletalContent->SetSkeleton(SkeletalContent->GetSkeletalMesh()->GetSkeleton());
		}
	}

	if(DataflowContent && DataflowContent->GetDataflowAsset())
	{
		DataflowContent->GetDataflowAsset()->Schema = UDataflowSchema::StaticClass();
	}
	
	// Potentially we could add additional objects to edit here (fields, meshes....)
	// If these objects have a matching factory we would be able to use geometry tools
	UBaseCharacterFXEditor::Initialize(RequiredObjects);
}

