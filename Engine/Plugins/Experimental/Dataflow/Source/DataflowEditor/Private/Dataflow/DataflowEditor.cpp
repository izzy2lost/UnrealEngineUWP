// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowEditor.h"

#include "Animation/Skeleton.h"
#include "Dataflow/DataflowEditorContent.h"
#include "Dataflow/DataflowEditorToolkit.h"
#include "Dataflow/DataflowEditorUtil.h"
#include "Engine/SkeletalMesh.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DataflowEditor)

DEFINE_LOG_CATEGORY(LogDataflowEditor);



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
	if (ensure(InObjects.Num() == 1))
	{
		TObjectPtr<UObject> RootObject = InObjects[0];
		Content->SetDataflowAsset(Cast<UDataflow>(RootObject));
		if(!Content->GetDataflowAsset())
		{
			Content->SetDataflowAsset(Private::GetDataflowAssetFrom(RootObject));
			Content->SetDataflowTerminal(Private::GetDataflowTerminalFrom(RootObject));
			Content->SetSkeletalMesh(Private::GetSkeletalMeshFrom(RootObject));
			Content->SetSkeleton(Private::GetSkeletonFrom(RootObject));
			Content->SetAnimationAsset(Private::GetAnimationAssetFrom(RootObject));
		}
		if(Content->GetDataflowAsset())
		{
			Content->SetDataflowOwner(RootObject);
			Content->GetDataflowAsset()->Schema = UDataflowSchema::StaticClass();
			Content->SetDataflowContext(MakeShared<Dataflow::FEngineContext>(RootObject, Content->GetDataflowAsset(), FPlatformTime::Cycles64()));
			Content->SetLastModifiedTimestamp(Content->GetDataflowContext()->GetTimestamp());

			ObjectsToEdit.Add(Content->GetDataflowOwner());

			if(!Content->GetSkeletalMesh())
			{
				const FName SkeletonName = MakeUniqueObjectName(Content->GetDataflowAsset(), UDataflow::StaticClass(), FName("USkeleton"));
				const FName SkeletalMeshName = MakeUniqueObjectName(Content->GetDataflowAsset(), UDataflow::StaticClass(), FName("USkeletalMesh"));

				Content->SetSkeleton(NewObject<USkeleton>(Content->GetDataflowAsset(), SkeletonName));
				Content->SetSkeletalMesh(NewObject<USkeletalMesh>(Content->GetDataflowAsset(), SkeletalMeshName));
				Content->GetSkeletalMesh()->SetSkeleton(Content->GetSkeleton());
			}
			else if(!Content->GetSkeleton())
			{
				Content->SetSkeleton(Content->GetSkeletalMesh()->GetSkeleton());
			}
		}
	}
	// Potentially we could add additional objects to edit here (fields, meshes....)
	// If these objects have a matching factory we would be able to use geometry tools
	UBaseCharacterFXEditor::Initialize(ObjectsToEdit);
}



