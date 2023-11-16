// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultiStreamColumns.h"

#include "IConcertClient.h"
#include "Replication/Client/ReplicationClient.h"
#include "Replication/Client/ReplicationClientManager.h"
#include "Replication/Editor/Model/IEditableMultiReplicationStreamModel.h"
#include "Replication/Editor/Model/IEditableReplicationStreamModel.h"
#include "Replication/Editor/View/IMultiReplicationStreamEditor.h"
#include "Replication/Editor/View/IReplicationStreamEditor.h"
#include "SAssignPropertyComboBox.h"
#include "Widgets/ActiveSession/Replication/Client/ClientUtils.h"

#define LOCTEXT_NAMESPACE "AssignPropertyColumn"

namespace UE::MultiUserClient::MultiStreamColumns
{
	const FName AssignPropertyColumnId(TEXT("AssignPropertyColumn"));

	namespace AssignPropertyColumnUtils
	{
		static void ForEachStreamAssignedTo(
			const ConcertClientSharedSlate::IMultiReplicationStreamEditor& MultiEditor,
			const FConcertPropertyChain& Property,
			TFunctionRef<void(const TSharedRef<ConcertClientSharedSlate::IEditableReplicationStreamModel>& Stream)> Consume
		)
		{
			for (const TSharedRef<ConcertClientSharedSlate::IEditableReplicationStreamModel>& Stream : MultiEditor.GetMultiStreamModel().GetEditableStreams())
			{
				for (const FSoftObjectPath& SelectedObject : MultiEditor.GetEditorBase().GetObjectsBeingPropertyEdited())
				{
					const bool bStreamAssignedToProperty = Stream->HasProperty(SelectedObject, Property);
					if (bStreamAssignedToProperty)
					{
						Consume(Stream);
						// Each IEditableReplicationStreamModel should only be visited at most once.
						break;
					}
				}
			}
		}

		const FReplicationClient* FindClientByStream(const FReplicationClientManager& ClientManager, const ConcertClientSharedSlate::IReplicationStreamModel& StreamModel)
		{
			if (&ClientManager.GetLocalClient().GetClientEditModel().Get() == &StreamModel)
			{
				return &ClientManager.GetLocalClient();
			}

			for (const TNonNullPtr<FRemoteReplicationClient> Client : ClientManager.GetRemoteClients())
			{
				if (&Client->GetClientEditModel().Get() == &StreamModel)
				{
					return Client;
				}
			}
			
			return nullptr;
		}
		
		static FString GetClientDisplayText(const IConcertClient& InConcertClient, const FReplicationClientManager& ClientManager, const ConcertClientSharedSlate::IReplicationStreamModel& StreamModel)
		{
			if (const FReplicationClient* Client = FindClientByStream(ClientManager, StreamModel))
			{
				return ClientUtils::GetClientDisplayName(InConcertClient, Client->GetEndpointId());
			}

			ensure(false);
			return {};
		}
	}
	
	ConcertClientSharedSlate::ReplicationColumns::FReplicationPropertyColumn AssignPropertyColumn(
		TAttribute<TSharedPtr<ConcertClientSharedSlate::IMultiReplicationStreamEditor>> MultiStreamEditor,
		TSharedRef<IConcertClient> ConcertClient,
		FReplicationClientManager& ClientManager,
		const int32 ColumnsSortPriority
		)
	{
		using namespace ConcertClientSharedSlate;
		using namespace ConcertClientSharedSlate::ReplicationColumns;
		check(MultiStreamEditor.IsBound() || MultiStreamEditor.IsSet());

		const auto PopuluateSearch = [MultiStreamEditor, &ClientManager, ConcertClient](const FReplicatedPropertyData& Data, TArray<FString>& InOutSearchStrings)
		{
			AssignPropertyColumnUtils::ForEachStreamAssignedTo(*MultiStreamEditor.Get(), Data.GetProperty(),
				[&ClientManager, &ConcertClient, &InOutSearchStrings](const TSharedRef<IEditableReplicationStreamModel>& Stream)
				{
					InOutSearchStrings.Add(AssignPropertyColumnUtils::GetClientDisplayText(*ConcertClient, ClientManager, *Stream));
				});
		};
		const auto GenerateWidgetColumn = [MultiStreamEditor, ConcertClient, &ClientManager](const FReplicationPropertyColumn::FBuildArgs& Args)
		{
			const TArray<FSoftObjectPath> DisplayedObjects = MultiStreamEditor.Get()->GetEditorBase().GetObjectsBeingPropertyEdited();
			return SNew(SAssignPropertyComboBox, MultiStreamEditor.Get().ToSharedRef(), ConcertClient, ClientManager)
				.DisplayedProperty(Args.RowData.GetProperty())
				.EditedObjects(DisplayedObjects)
				.HighlightText(Args.HighlightText);
		};
		
		return FReplicationPropertyColumn(
			typename FReplicationPropertyColumn::FArguments()
				.PopulateSearchItems_Lambda(PopuluateSearch)
				.GenerateWidgetColumn_Lambda(GenerateWidgetColumn)
				.ColumnSortOrder(ColumnsSortPriority),
			SHeaderRow::Column(AssignPropertyColumnId)
				.DefaultLabel(LOCTEXT("Owner", "Owner"))
				.FillSized(200.f)
			);
	}
}

#undef LOCTEXT_NAMESPACE