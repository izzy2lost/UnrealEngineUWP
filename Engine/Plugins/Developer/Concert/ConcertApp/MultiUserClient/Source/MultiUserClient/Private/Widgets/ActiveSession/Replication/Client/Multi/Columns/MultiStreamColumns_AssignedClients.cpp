// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultiStreamColumns.h"

#include "IConcertClient.h"
#include "MultiUserReplicationStyle.h"
#include "Replication/Editor/View/IMultiReplicationStreamEditor.h"
#include "Replication/Submission/MultiEdit/ReassignObjectPropertiesLogic.h"
#include "Widgets/ActiveSession/Replication/Client/ClientUtils.h"
#include "Widgets/Client/ClientInfoHelpers.h"
#include "Widgets/Client/SHorizontalClientList.h"

#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "AssignedClientsColumnId"

namespace UE::MultiUserClient::MultiStreamColumns
{
	const FName AssignedClientsColumnId(TEXT("AssignedClientsColumn"));

	namespace Private
	{
		static TArray<FGuid> GetDisplayedClients(const FReassignObjectPropertiesLogic& ReassignmentLogic, const FSoftObjectPath& ManagedObject)
		{
			TArray<FGuid> ClientsWithOwnership;
			ReassignmentLogic.EnumerateClientOwnershipState(ManagedObject, [&ClientsWithOwnership](const FGuid& ClientId, FReassignObjectPropertiesLogic::EOwnershipState Ownership)
			{
				if (Ownership == FReassignObjectPropertiesLogic::EOwnershipState::HasObjectRegistered)
				{
					ClientsWithOwnership.Add(ClientId);
				}
				return EBreakBehavior::Continue;
			});
			return ClientsWithOwnership;
		}
		
		class SAssignedClientsWidget : public SCompoundWidget
		{
			TSharedPtr<ConcertClientSharedSlate::SHorizontalClientList> ClientList;
			FSoftObjectPath ManagedObject;
			FReassignObjectPropertiesLogic* ReassignmentLogic = nullptr;
		public:
		
			SLATE_BEGIN_ARGS(SAssignedClientsWidget){}
			SLATE_END_ARGS()

			void Construct(
				const FArguments& InArgs,
				const TSharedRef<IConcertClient>& InConcertClient,
				FReassignObjectPropertiesLogic& InReassignmentLogic,
				FSoftObjectPath InManagedObject,
				TSharedPtr<FText> InHighlightText
				)
			{
				ReassignmentLogic = &InReassignmentLogic;
				ManagedObject = MoveTemp(InManagedObject);
			
				ChildSlot
				[
					SAssignNew(ClientList, ConcertClientSharedSlate::SHorizontalClientList)
					.IsLocalClient(ConcertClientSharedSlate::MakeIsLocalClientGetter(InConcertClient))
					.GetClientInfo(ConcertClientSharedSlate::MakeClientInfoGetter(InConcertClient))
					.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
					.HighlightText_Lambda([InHighlightText](){ return InHighlightText ? *InHighlightText : FText::GetEmpty(); })
					.ListToolTipText(LOCTEXT("Clients.ToolTip", "These clients will replicate their assigned properties when replication is active.\nYou can pause & resume replication at the beginnig of this row."))
					.EmptyListSlot()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("NoClients.Label", "No assigned properties"))
						.ToolTipText(LOCTEXT("NoClients.ToolTip", "Click this row and then assign the properties to the client that should replicate them."))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					]
				];
			
				UpdateComboButtonContent();
				ReassignmentLogic->OnOwnershipChanged().AddSP(this, &SAssignedClientsWidget::UpdateComboButtonContent);
			}
		
			virtual ~SAssignedClientsWidget() override
			{
				ReassignmentLogic->OnOwnershipChanged().RemoveAll(this);
			}

			void UpdateComboButtonContent() const
			{
				ClientList->RefreshList(
					GetDisplayedClients(*ReassignmentLogic, ManagedObject)
					);
			}
		};
	}
	
	ConcertSharedSlate::FObjectColumnEntry AssignedClientsColumn(
		TSharedRef<IConcertClient> ConcertClient,
		TAttribute<TSharedPtr<ConcertSharedSlate::IMultiReplicationStreamEditor>> MultiStreamModelAttribute,
		TAttribute<ConcertSharedSlate::IObjectHierarchyModel*> ObjectHierarchyModelAttribute,
		FReassignObjectPropertiesLogic& ReassignmentLogic,
		const FReplicationClientManager& ClientManager,
		const int32 ColumnsSortPriority
		)
	{
		class FObjectColumn_ReassignOwnership : public ConcertSharedSlate::IObjectTreeColumn
		{
		public:

			FObjectColumn_ReassignOwnership(
				TSharedRef<IConcertClient> ConcertClient,
				TAttribute<TSharedPtr<ConcertSharedSlate::IMultiReplicationStreamEditor>> MultiStreamModelAttribute,
				TAttribute<ConcertSharedSlate::IObjectHierarchyModel*> ObjectHierarchyModelAttribute,
				FReassignObjectPropertiesLogic& ReassignmentLogic,
				const FReplicationClientManager& ClientManager
				)
				: ConcertClient(MoveTemp(ConcertClient))
				, MultiStreamModelAttribute(MoveTemp(MultiStreamModelAttribute))
				, ObjectHierarchyModelAttribute(MoveTemp(ObjectHierarchyModelAttribute))
				, ReassignmentLogic(ReassignmentLogic)
				, ClientManager(ClientManager)
			{}
			
			virtual SHeaderRow::FColumn::FArguments CreateHeaderRowArgs() const override
			{
				return SHeaderRow::Column(AssignedClientsColumnId)
					.DefaultLabel(LOCTEXT("Owner.Label", "Clients"))
					.ToolTipText(LOCTEXT("Owner.ToolTip", "Clients that have registered properties for the object"))
					.FillSized(FMultiUserReplicationStyle::Get()->GetFloat(TEXT("AllClients.Object.OwnerSize")));
			}
			
			virtual TSharedRef<SWidget> GenerateColumnWidget(const FBuildArgs& InArgs) override
			{
				return SNew(Private::SAssignedClientsWidget, ConcertClient, ReassignmentLogic, InArgs.RowItem.RowData.GetObjectPath(), InArgs.HighlightText);
			}
			virtual void PopulateSearchString(const ConcertSharedSlate::FObjectTreeRowContext& InItem, TArray<FString>& InOutSearchStrings) const override
			{
				for (const FGuid& ClientId : Private::GetDisplayedClients(ReassignmentLogic, InItem.RowData.GetObjectPath()))
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
			const TAttribute<ConcertSharedSlate::IObjectHierarchyModel*> ObjectHierarchyModelAttribute;
			FReassignObjectPropertiesLogic& ReassignmentLogic;
			const FReplicationClientManager& ClientManager;
			
			TOptional<FString> GetDisplayString(const FSoftObjectPath& ManagedObject) const
			{
				using SWidgetType = ConcertClientSharedSlate::SHorizontalClientList;
				const TArray<FGuid> Clients = Private::GetDisplayedClients(ReassignmentLogic, ManagedObject);
				const ConcertSharedSlate::FIsLocalClient IsLocalClientDelegate = ConcertClientSharedSlate::MakeIsLocalClientGetter(ConcertClient);
				return SWidgetType::GetDisplayString(
					Clients,
					ConcertClientSharedSlate::MakeClientInfoGetter(ConcertClient),
					SWidgetType::FSortPredicate::CreateStatic(&SWidgetType::SortLocalClientFirstThenAlphabetical, IsLocalClientDelegate),
					IsLocalClientDelegate
					);
			}
		};

		return {
			ConcertSharedSlate::TReplicationColumnDelegates<ConcertSharedSlate::FObjectTreeRowContext>::FCreateColumn::CreateLambda(
				[ConcertClient = MoveTemp(ConcertClient), MultiStreamModelAttribute = MoveTemp(MultiStreamModelAttribute), ObjectHierarchyModelAttribute = MoveTemp(ObjectHierarchyModelAttribute), &ReassignmentLogic, &ClientManager]()
				{
					return MakeShared<FObjectColumn_ReassignOwnership>(ConcertClient, MultiStreamModelAttribute, ObjectHierarchyModelAttribute, ReassignmentLogic, ClientManager);
				}),
			AssignedClientsColumnId,
			{ ColumnsSortPriority }
		};
	}
}

#undef LOCTEXT_NAMESPACE