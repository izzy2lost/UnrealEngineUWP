// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultiStreamColumns.h"

#include "IConcertClient.h"
#include "MultiUserReplicationStyle.h"
#include "Replication/Editor/Model/Object/IObjectHierarchyModel.h"
#include "Replication/Editor/View/IMultiReplicationStreamEditor.h"
#include "Replication/Submission/MultiEdit/ReassignObjectPropertiesLogic.h"
#include "Widgets/ActiveSession/Replication/Client/ClientUtils.h"
#include "Widgets/Client/ClientInfoHelpers.h"
#include "Widgets/Client/SHorizontalClientList.h"

#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "AssignedClientsColumnId"

namespace UE::MultiUserClient::Replication::MultiStreamColumns
{
	const FName AssignedClientsColumnId(TEXT("AssignedClientsColumn"));

	namespace Private
	{
		static TArray<FGuid> GetDisplayedClients(
			const ConcertSharedSlate::IObjectHierarchyModel& ObjectHierarchy,
			const FReassignObjectPropertiesLogic& ReassignmentLogic,
			const FSoftObjectPath& ManagedObject
			)
		{
			TArray<FGuid> ClientsWithOwnership;
			const auto ProcessObject = [&ReassignmentLogic, &ClientsWithOwnership](const FSoftObjectPath& ObjectPath)
			{
				ReassignmentLogic.EnumerateClientOwnershipState(ObjectPath, [&ClientsWithOwnership](const FGuid& ClientId, FReassignObjectPropertiesLogic::EOwnershipState Ownership)
				{
					if (Ownership == FReassignObjectPropertiesLogic::EOwnershipState::HasObjectRegistered)
					{
						ClientsWithOwnership.AddUnique(ClientId);
					}
					return EBreakBehavior::Continue;
				});
			};
			
			ProcessObject(ManagedObject);
			ObjectHierarchy.ForEachChildRecursive(
				TSoftObjectPtr<>{ ManagedObject },
				[&ProcessObject](const TSoftObjectPtr<>&, const TSoftObjectPtr<>& ChildObject, ConcertSharedSlate::EChildRelationship)
				{
					ProcessObject(ChildObject.GetUniqueID());
					return EBreakBehavior::Continue;
				});
			
			return ClientsWithOwnership;
		}
		
		class SAssignedClientsWidget : public SCompoundWidget
		{
			TSharedPtr<ConcertClientSharedSlate::SHorizontalClientList> ClientList;
			FSoftObjectPath ManagedObject;
			const ConcertSharedSlate::IObjectHierarchyModel* ObjectHierarchy = nullptr;
			FReassignObjectPropertiesLogic* ReassignmentLogic = nullptr;
			
		public:
		
			SLATE_BEGIN_ARGS(SAssignedClientsWidget){}
				SLATE_ARGUMENT(FSoftObjectPath, ManagedObject)
				SLATE_ATTRIBUTE(FText, HighlightText)
			SLATE_END_ARGS()

			void Construct(
				const FArguments& InArgs,
				const TSharedRef<IConcertClient>& InConcertClient,
				const ConcertSharedSlate::IObjectHierarchyModel& InObjectHierarchy,
				FReassignObjectPropertiesLogic& InReassignmentLogic
				)
			{
				ObjectHierarchy = &InObjectHierarchy;
				ReassignmentLogic = &InReassignmentLogic;
				ManagedObject = InArgs._ManagedObject;
			
				ChildSlot
				[
					SAssignNew(ClientList, ConcertClientSharedSlate::SHorizontalClientList)
					.IsLocalClient(ConcertClientSharedSlate::MakeIsLocalClientGetter(InConcertClient))
					.GetClientInfo(ConcertClientSharedSlate::MakeClientInfoGetter(InConcertClient))
					.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
					.HighlightText(InArgs._HighlightText)
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
					GetDisplayedClients(*ObjectHierarchy, *ReassignmentLogic, ManagedObject)
					);
			}
		};
	}

	ConcertSharedSlate::FObjectColumnEntry AssignedClientsColumn(
		TSharedRef<IConcertClient> ConcertClient,
		TAttribute<TSharedPtr<ConcertSharedSlate::IMultiReplicationStreamEditor>> MultiStreamModelAttribute,
		const ConcertSharedSlate::IObjectHierarchyModel& ObjectHierarchy,
		FReassignObjectPropertiesLogic& ReassignmentLogic,
		const FOnlineClientManager& ClientManager,
		const int32 ColumnsSortPriority
		)
	{
		class FObjectColumn_ReassignOwnership : public ConcertSharedSlate::IObjectTreeColumn
		{
		public:

			FObjectColumn_ReassignOwnership(
				TSharedRef<IConcertClient> ConcertClient,
				TAttribute<TSharedPtr<ConcertSharedSlate::IMultiReplicationStreamEditor>> MultiStreamModelAttribute,
				const ConcertSharedSlate::IObjectHierarchyModel& ObjectHierarchy UE_LIFETIMEBOUND,
				FReassignObjectPropertiesLogic& ReassignmentLogic UE_LIFETIMEBOUND,
				const FOnlineClientManager& ClientManager UE_LIFETIMEBOUND
				)
				: ConcertClient(MoveTemp(ConcertClient))
				, MultiStreamModelAttribute(MoveTemp(MultiStreamModelAttribute))
				, ObjectHierarchy(ObjectHierarchy)
				, ReassignmentLogic(ReassignmentLogic)
				, ClientManager(ClientManager)
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
				return SNew(Private::SAssignedClientsWidget, ConcertClient, ObjectHierarchy, ReassignmentLogic)
					.ManagedObject(InArgs.RowItem.RowData.GetObjectPath())
					.HighlightText_Lambda([HighlightText = InArgs.HighlightText](){ return HighlightText ? *HighlightText : FText::GetEmpty(); });
			}
			virtual void PopulateSearchString(const ConcertSharedSlate::FObjectTreeRowContext& InItem, TArray<FString>& InOutSearchStrings) const override
			{
				for (const FGuid& ClientId : Private::GetDisplayedClients(ObjectHierarchy, ReassignmentLogic, InItem.RowData.GetObjectPath()))
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
			const ConcertSharedSlate::IObjectHierarchyModel& ObjectHierarchy;
			FReassignObjectPropertiesLogic& ReassignmentLogic;
			const FOnlineClientManager& ClientManager;
			
			TOptional<FString> GetDisplayString(const FSoftObjectPath& ManagedObject) const
			{
				using SWidgetType = ConcertClientSharedSlate::SHorizontalClientList;
				const TArray<FGuid> Clients = Private::GetDisplayedClients(ObjectHierarchy, ReassignmentLogic, ManagedObject);
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
				[ConcertClient = MoveTemp(ConcertClient), MultiStreamModelAttribute = MoveTemp(MultiStreamModelAttribute), &ObjectHierarchy, &ReassignmentLogic, &ClientManager]()
				{
					return MakeShared<FObjectColumn_ReassignOwnership>(ConcertClient, MultiStreamModelAttribute, ObjectHierarchy, ReassignmentLogic, ClientManager);
				}),
			AssignedClientsColumnId,
			{ ColumnsSortPriority }
		};
	}
}

#undef LOCTEXT_NAMESPACE