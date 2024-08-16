// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssignedClientsModel.h"

#include "IConcertClient.h"
#include "MultiUserReplicationStyle.h"
#include "Replication/Client/ClientUtils.h"
#include "Replication/Editor/View/IMultiReplicationStreamEditor.h"
#include "SAssignedClientsWidget.h"
#include "Widgets/ActiveSession/Replication/Client/Multi/Columns/MultiStreamColumns.h"
#include "Widgets/Client/ClientInfoHelpers.h"
#include "Widgets/Client/SHorizontalClientList.h"

#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "AssignedClientsColumnId"

namespace UE::MultiUserClient::Replication::MultiStreamColumns
{
	const FName AssignedClientsColumnId(TEXT("AssignedClientsColumn"));

	ConcertSharedSlate::FObjectColumnEntry AssignedClientsColumn(
		TSharedRef<IConcertClient> ConcertClient,
		TAttribute<TSharedPtr<ConcertSharedSlate::IMultiReplicationStreamEditor>> MultiStreamModelAttribute,
		const ConcertSharedSlate::IObjectHierarchyModel& ObjectHierarchy,
		FOnlineClientManager& ClientManager,
		const int32 ColumnsSortPriority
		)
	{
		class FObjectColumn_AssignedClients : public ConcertSharedSlate::IObjectTreeColumn
		{
		public:

			FObjectColumn_AssignedClients(
				TSharedRef<IConcertClient> ConcertClient,
				TAttribute<TSharedPtr<ConcertSharedSlate::IMultiReplicationStreamEditor>> MultiStreamModelAttribute,
				const ConcertSharedSlate::IObjectHierarchyModel& ObjectHierarchy UE_LIFETIMEBOUND,
				FOnlineClientManager& ClientManager UE_LIFETIMEBOUND
				)
				: ConcertClient(MoveTemp(ConcertClient))
				, MultiStreamModelAttribute(MoveTemp(MultiStreamModelAttribute))
				, Model(ObjectHierarchy, ClientManager)
			{}
			
			virtual SHeaderRow::FColumn::FArguments CreateHeaderRowArgs() const override
			{
				return SHeaderRow::Column(AssignedClientsColumnId)
					.DefaultLabel(LOCTEXT("Owner.Label", "Author"))
					.ToolTipText(LOCTEXT("Owner.ToolTip", "Clients that have registered properties for the object"))
					.FillSized(FMultiUserReplicationStyle::Get()->GetFloat(TEXT("AllClients.Object.OwnerSize")));
			}
			
			virtual TSharedRef<SWidget> GenerateColumnWidget(const FBuildArgs& InArgs) override
			{
				return SNew(SAssignedClientsWidget, ConcertClient, Model)
					.ManagedObject(InArgs.RowItem.RowData.GetObjectPath())
					.HighlightText_Lambda([HighlightText = InArgs.HighlightText](){ return HighlightText ? *HighlightText : FText::GetEmpty(); });
			}
			virtual void PopulateSearchString(const ConcertSharedSlate::FObjectTreeRowContext& InItem, TArray<FString>& InOutSearchStrings) const override
			{
				for (const FGuid& ClientId : Model.GetAssignedClients(InItem.RowData.GetObjectPath()))
				{
					InOutSearchStrings.Add(ClientUtils::GetClientDisplayName(*ConcertClient->GetCurrentSession(), ClientId));
				}
			}

			virtual bool CanBeSorted() const override { return true; }
			virtual bool IsLessThan(const ConcertSharedSlate::FObjectTreeRowContext& Left, const ConcertSharedSlate::FObjectTreeRowContext& Right) const override
			{
				const TOptional<FString> LeftClientDisplayString = GetDisplayString(Left.RowData.GetObjectPath());
				const TOptional<FString> RightClientDisplayString = GetDisplayString(Right.RowData.GetObjectPath());
			
				if (LeftClientDisplayString && RightClientDisplayString)
				{
					return *LeftClientDisplayString < *RightClientDisplayString;
				}
				// Our rule: set < unset. This way unassigned appears last.
				return LeftClientDisplayString.IsSet() && !RightClientDisplayString.IsSet();
			}

		private:
			
			const TSharedRef<IConcertClient> ConcertClient;
			const TAttribute<TSharedPtr<ConcertSharedSlate::IMultiReplicationStreamEditor>> MultiStreamModelAttribute;

			/** The model that the view displays; the model of the MVC pattern. */
			FAssignedClientsModel Model;
			
			TOptional<FString> GetDisplayString(const FSoftObjectPath& ManagedObject) const
			{
				using SWidgetType = ConcertSharedSlate::SHorizontalClientList;
				const TArray<FGuid> Clients = Model.GetAssignedClients(ManagedObject);

				const ConcertSharedSlate::FGetClientParenthesesContent GetParenthesesContent =
					ConcertClientSharedSlate::MakeGetLocalClientParenthesesContent(ConcertClient);
				const auto SortPredicate = [&GetParenthesesContent](const FConcertSessionClientInfo& Left, const FConcertSessionClientInfo& Right)
				{
					return ConcertSharedSlate::SortLocalClientParenthesesFirstThenThenAlphabetical(Left, Right, GetParenthesesContent);
				};
				return SWidgetType::GetDisplayString(
					Clients,
					ConcertClientSharedSlate::MakeClientInfoGetter(ConcertClient),
					ConcertSharedSlate::FClientSortPredicate::CreateLambda(SortPredicate),
					GetParenthesesContent
					);
			}
		};

		return {
			ConcertSharedSlate::TReplicationColumnDelegates<ConcertSharedSlate::FObjectTreeRowContext>::FCreateColumn::CreateLambda(
				[ConcertClient = MoveTemp(ConcertClient), MultiStreamModelAttribute = MoveTemp(MultiStreamModelAttribute), &ObjectHierarchy, &ClientManager]()
				{
					return MakeShared<FObjectColumn_AssignedClients>(ConcertClient, MultiStreamModelAttribute, ObjectHierarchy, ClientManager);
				}),
			AssignedClientsColumnId,
			{ ColumnsSortPriority }
		};
	}
}

#undef LOCTEXT_NAMESPACE