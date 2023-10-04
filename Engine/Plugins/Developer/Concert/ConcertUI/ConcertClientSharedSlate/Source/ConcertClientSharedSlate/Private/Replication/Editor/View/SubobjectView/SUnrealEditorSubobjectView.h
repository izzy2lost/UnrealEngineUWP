// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Editor/View/IReplicationSubobjectView.h"

namespace UE::ConcertClientSharedSlate
{
	class SConcertReplicationSubobjectEditor;

	/** Adapts SSubobjectEditor to the IReplicationSubobjectView interface. */
	class SUnrealEditorSubobjectView : public IReplicationSubobjectView
	{
	public:

		SLATE_BEGIN_ARGS(SUnrealEditorSubobjectView)
		{}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);

		//~ Begin IReplicationSubobjectView Interface
		virtual void SetRootObjects(const TArray<FSoftObjectPath>& RootObjects) override;
		virtual void SelectRootObjects() override;
		virtual TArray<FSoftObjectPath> GetSelectedObjects() const override;
		virtual FOnSelectionChanged& OnSelectionChanged() override { return OnSelectionChangedDelegate; }
		//~ End IReplicationSubobjectView Interface

	private:

		/** Reuses the SSubobjectEditor for displaying subobjects. */
		TSharedPtr<SConcertReplicationSubobjectEditor> SubobjectEditor;

		/** Executes when the selection is changed. */
		FOnSelectionChanged OnSelectionChangedDelegate;

		/** Called when the SSubobjectEditor reports new selected objects. */
		void OnSubobjectsSelected();
	};
}

