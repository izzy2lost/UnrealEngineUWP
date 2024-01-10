// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultiStreamColumns.h"

#include "IConcertClient.h"
#include "MultiUserReplicationStyle.h"
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
			const ConcertSharedSlate::IMultiReplicationStreamEditor& MultiEditor,
			const FConcertPropertyChain& Property,
			TFunctionRef<void(const TSharedRef<ConcertSharedSlate::IEditableReplicationStreamModel>& Stream)> Consume
		)
		{
			for (const TSharedRef<ConcertSharedSlate::IEditableReplicationStreamModel>& Stream : MultiEditor.GetMultiStreamModel().GetEditableStreams())
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

		const FReplicationClient* FindClientByStream(const FReplicationClientManager& ClientManager, const ConcertSharedSlate::IReplicationStreamModel& StreamModel)
		{
			if (&ClientManager.GetLocalClient().GetClientEditModel().Get() == &StreamModel)
			{
				return &ClientManager.GetLocalClient();
			}

			for (const TNonNullPtr<const FRemoteReplicationClient> Client : ClientManager.GetRemoteClients())
			{
				if (&Client->GetClientEditModel().Get() == &StreamModel)
				{
					return Client;
				}
			}
			
			return nullptr;
		}
		
		static FString GetClientDisplayText(const IConcertClient& InConcertClient, const FReplicationClientManager& ClientManager, const ConcertSharedSlate::IReplicationStreamModel& StreamModel)
		{
			if (const FReplicationClient* Client = FindClientByStream(ClientManager, StreamModel))
			{
				return ClientUtils::GetClientDisplayName(InConcertClient, Client->GetEndpointId());
			}

			ensure(false);
			return {};
		}
	}
	
	ConcertSharedSlate::ReplicationColumns::FReplicationPropertyColumn AssignPropertyColumn(
		TAttribute<TSharedPtr<ConcertSharedSlate::IMultiReplicationStreamEditor>> MultiStreamEditor,
		TSharedRef<IConcertClient> ConcertClient,
		FReplicationClientManager& ClientManager,
		const int32 ColumnsSortPriority
		)
	{
		using namespace ConcertSharedSlate;
		using namespace ConcertSharedSlate::ReplicationColumns;
		check(MultiStreamEditor.IsBound() || MultiStreamEditor.IsSet());

		const auto PopulateSearch = [MultiStreamEditor, &ClientManager, ConcertClient](const FReplicatedPropertyData& Data, TArray<FString>& InOutSearchStrings)
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
				.HighlightText(Args.HighlightText)
				.OnOptionSelected_Lambda([MultiStreamEditor](auto)
				{
					if (const TSharedPtr<IMultiReplicationStreamEditor> Editor = MultiStreamEditor.Get())
					{
						Editor->GetEditorBase().RequestPropertyColumnResort(AssignPropertyColumnId);
					}
				});
		};
		const auto IsLessThan = [MultiStreamEditor, ConcertClient, &ClientManager](const FReplicatedPropertyData& Left, const FReplicatedPropertyData& Right)
		{
			const TArray<FSoftObjectPath> DisplayedObjects = MultiStreamEditor.Get()->GetEditorBase().GetObjectsBeingPropertyEdited();
			const TOptional<FString> LeftClientDisplayString = SAssignPropertyComboBox::GetDisplayString(ConcertClient, ClientManager, Left.GetProperty(), DisplayedObjects);
			const TOptional<FString> RightClientDisplayString = SAssignPropertyComboBox::GetDisplayString(ConcertClient, ClientManager, Right.GetProperty(), DisplayedObjects);
			
			if (LeftClientDisplayString && RightClientDisplayString)
			{
				return *LeftClientDisplayString < *RightClientDisplayString;
			}
			// Our rule: set < unset. This way unassigned appears last.
			return LeftClientDisplayString.IsSet() && !RightClientDisplayString.IsSet();
		};
		
		return FReplicationPropertyColumn(
			typename FReplicationPropertyColumn::FArguments()
				.PopulateSearchItems_Lambda(PopulateSearch)
				.GenerateWidgetColumn_Lambda(GenerateWidgetColumn)
				.IsLessThan_Lambda(IsLessThan)
				.ColumnSortOrder(ColumnsSortPriority),
			SHeaderRow::Column(AssignPropertyColumnId)
				.DefaultLabel(LOCTEXT("Owner.Label", "Assigned Client"))
				.ToolTipText(LOCTEXT("Owner.ToolTip", "Client that should replicate this property"))
				.FillSized(FMultiUserReplicationStyle::Get()->GetFloat(TEXT("AllClients.Property.OwnerColumnWidth")))
			);
	}
}

#undef LOCTEXT_NAMESPACE