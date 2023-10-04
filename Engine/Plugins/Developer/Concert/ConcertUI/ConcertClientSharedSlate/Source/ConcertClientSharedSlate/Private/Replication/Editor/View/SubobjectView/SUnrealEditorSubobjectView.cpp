// Copyright Epic Games, Inc. All Rights Reserved.

#include "SUnrealEditorSubobjectView.h"

#include "SConcertReplicationSubobjectEditor.h"

namespace UE::ConcertClientSharedSlate
{
	void SUnrealEditorSubobjectView::Construct(const FArguments& InArgs)
	{
		ChildSlot
		[
			SAssignNew(SubobjectEditor, SConcertReplicationSubobjectEditor)
			.OnSubobjectsSelected(this, &SUnrealEditorSubobjectView::OnSubobjectsSelected)
		];
	}
	
	void SUnrealEditorSubobjectView::SetRootObjects(const TArray<FSoftObjectPath>& RootObject)
	{
		if (RootObject.IsEmpty())
		{
			SubobjectEditor->SetDisplayedRootObject(nullptr);
		}
		else
		{
			// TODO DP: Check whether all of the objects are compatible with each other
			UObject* Object = RootObject[0].ResolveObject();
			SubobjectEditor->SetDisplayedRootObject(Object);
		}
	}

	void SUnrealEditorSubobjectView::SelectRootObjects()
	{
		SubobjectEditor->SelectRoot();
	}

	TArray<FSoftObjectPath> SUnrealEditorSubobjectView::GetSelectedObjects() const
	{
		TArray<FSoftObjectPath> Paths;
		Algo::Transform(SubobjectEditor->GetSelectedObjects(), Paths, [](const UObject* Object){ return Object; });
		return Paths;
	}

	void SUnrealEditorSubobjectView::OnSubobjectsSelected()
	{
		OnSelectionChangedDelegate.Broadcast();
	}
}
