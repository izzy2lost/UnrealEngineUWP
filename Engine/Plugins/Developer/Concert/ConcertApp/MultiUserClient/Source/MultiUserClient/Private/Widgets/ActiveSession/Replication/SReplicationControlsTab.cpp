// Copyright Epic Games, Inc. All Rights Reserved.

#include "SReplicationControlsTab.h"

#include "Assets/MultiUserReplicationSessionPreset.h"
#include "Replication/MultiUserReplicationManager.h"
#include "Replication/ReplicationWidgetFactories.h"
#include "Replication/Editor/Model/Object/EditorObjectSelectionSourceModel.h"
#include "Replication/Editor/Model/Property/SelectPropertyFromUClassModel.h"

#include "Styling/AppStyle.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SReplicationControlsTab"

namespace UE::MultiUserClient
{
	void SReplicationControlsTab::Construct(const FArguments& InArgs, TSharedRef<FMultiUserReplicationManager> InReplicationManager)
	{
		const UMultiUserReplicationClientPreset* LocalClient = InReplicationManager->GetLocalClientContent();
		check(LocalClient && LocalClient->Stream);

		using namespace ConcertClientSharedSlate;
		const FCreateEditorParams ReplicationEditorCreationParams
		{
			CreatePropertySelectionModel(*LocalClient->Stream, LocalClient->Stream->MakeReplicationMapGetterAttribute()),
			MakeShared<FEditorObjectSelectionSourceModel>(),
			MakeShared<FSelectPropertyFromUClassModel>()
		};
		ChildSlot
		[
			CreateEditor(ReplicationEditorCreationParams)
		];
	}
}

#undef LOCTEXT_NAMESPACE