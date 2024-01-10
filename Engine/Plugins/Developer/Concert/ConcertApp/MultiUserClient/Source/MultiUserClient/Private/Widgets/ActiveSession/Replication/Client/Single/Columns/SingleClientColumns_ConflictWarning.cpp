// Copyright Epic Games, Inc. All Rights Reserved.

#include "SingleClientColumns.h"

#include "IConcertClient.h"
#include "Replication/Authority/EAuthorityMutability.h"
#include "Replication/Editor/Model/ReplicatedObjectData.h"
#include "Replication/Editor/View/IReplicationStreamViewer.h"
#include "Replication/Util/GlobalAuthorityCache.h"
#include "Widgets/ActiveSession/Replication/Client/ClientUtils.h"
#include "Widgets/ActiveSession/Replication/Client/SWarningIcon.h"

#include "Algo/AllOf.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Views/SHeaderRow.h"

#define LOCTEXT_NAMESPACE "SingleClientColumns.ConflictWarning"

namespace UE::MultiUserClient::SingleClientColumns
{
	const FName ConflictWarningTopLevelObjectColumnId = TEXT("ConflictWarningTopLevelObjectColumn");
	const FName ConflictWarningSubobjectColumnId = TEXT("ConflictWarningSubobjectColumn");
	const FName ConflictWarningPropertyColumnId = TEXT("ConflictWarningPropertyColumn");
	
	ConcertSharedSlate::ReplicationColumns::FReplicationTopLevelObjectColumn ConflictWarningForObject(
		TSharedRef<IConcertClient> InClient,
		FGlobalAuthorityCache& InAuthorityCache,
		const FGuid& ClientId
		)
	{
		using FColumnType = ConcertSharedSlate::ReplicationColumns::FReplicationTopLevelObjectColumn;
		return FColumnType(
			FColumnType::FArguments()
				.GenerateWidgetColumn_Lambda([InClient = MoveTemp(InClient), &InAuthorityCache, ClientId](const FColumnType::FBuildArgs& InArgs) mutable
				{
					const auto GetVisibility = [&InAuthorityCache, ClientId, ObjectPath = InArgs.RowData.GetObjectPath()]()
					{
						const EAuthorityMutability TakeAuthorityResult = InAuthorityCache.CanClientTakeAuthorityAfterSubmission(ObjectPath, ClientId);
						return TakeAuthorityResult == EAuthorityMutability::Conflict
							? EVisibility::Visible
							: EVisibility::Collapsed;
					};
					const auto GetToolTip = [InClient = MoveTemp(InClient), &InAuthorityCache, ClientId, ObjectPath = InArgs.RowData.GetObjectPath()]()
					{
						TSet<FString> ClientNames;
						const EAuthorityMutability TakeAuthorityResult = InAuthorityCache.CanClientTakeAuthorityAfterSubmission(ObjectPath, ClientId,
						[InClient, &ClientNames](const FGuid& ClientId, const FConcertPropertyChain& ConflictingProperty)
						{
							ClientNames.Add(ClientUtils::GetClientDisplayName(*InClient, ClientId));
							return EBreakBehavior::Continue;
						});
						
						return TakeAuthorityResult != EAuthorityMutability::Conflict
							? FText::GetEmpty()
							: FText::Format(
								LOCTEXT("Subobject.WarningFmt", "{0} {1}|plural(one=is,other=are) replicating some of the assigned properties. This change will not be sent to the server."),
								FText::FromString(FString::Join(ClientNames, TEXT(", "))),
								ClientNames.Num()
							);
					};
					
					return SNew(SWarningIcon)
						.Visibility_Lambda(GetVisibility)
						.ToolTipText_Lambda(GetToolTip);
				})
				.PopulateSearchItems_Lambda([](const ConcertSharedSlate::FReplicatedObjectData&, TArray<FString>&){})
				.ColumnSortOrder(static_cast<int32>(ETopLevelObjectColumnOrder::ConflictWarning)),
			SHeaderRow::Column(ConflictWarningSubobjectColumnId)
				.DefaultLabel(FText::GetEmpty())
				.FixedWidth(20.f)
			);
	}

	ConcertSharedSlate::ReplicationColumns::FReplicationPropertyColumn ConflictWarningForProperty(
		TSharedRef<IConcertClient> InClient,
		TAttribute<const ConcertSharedSlate::IReplicationStreamViewer*> Viewer,
		FGlobalAuthorityCache& InAuthorityCache,
		const FGuid& ClientId
		)
	{
		using FColumnType = ConcertSharedSlate::ReplicationColumns::FReplicationPropertyColumn;
		return FColumnType(
			FColumnType::FArguments()
				.GenerateWidgetColumn_Lambda([InClient = MoveTemp(InClient), Viewer, &InAuthorityCache, ClientId](const FColumnType::FBuildArgs& InArgs) mutable
				{
					const auto GetVisibility = [&Viewer, &InAuthorityCache, ClientId, Property = InArgs.RowData.GetProperty()]()
					{
						const bool bCanAddProperty = Algo::AllOf(Viewer.Get()->GetObjectsBeingPropertyEdited(),
							[&InAuthorityCache, ClientId, Property](const FSoftObjectPath& ObjectPath)
							{
								return InAuthorityCache.CanClientAddProperty(ObjectPath, ClientId, Property);
							});
						return bCanAddProperty
							? EVisibility::Collapsed
							: EVisibility::Visible;
					};
					const auto GetToolTip = [InClient = MoveTemp(InClient), Viewer, &InAuthorityCache, ClientId, Property = InArgs.RowData.GetProperty()]()
					{
						TSet<FString> Names;
						for (const FSoftObjectPath& ObjectPath: Viewer.Get()->GetObjectsBeingPropertyEdited())
						{
							const TOptional<FGuid> Owner = InAuthorityCache.GetClientWithAuthorityOverProperty(ObjectPath, Property);
							if (Owner || *Owner != ClientId)
							{
								Names.Add(ClientUtils::GetClientDisplayName(*InClient, *Owner));
							}
						}
						
						return Names.IsEmpty()
							? FText::GetEmpty()
							: FText::Format(
								LOCTEXT("Property.WarningFmt", "Replicated by {0}. This property will not be submitted to the server."),
								FText::FromString(FString::Join(Names, TEXT(", ")))
								);
					};
					
					return SNew(SWarningIcon)
						.Visibility_Lambda(GetVisibility)
						.ToolTipText_Lambda(GetToolTip);
				})
				.PopulateSearchItems_Lambda([](const ConcertSharedSlate::FReplicatedPropertyData&, TArray<FString>&){})
				.ColumnSortOrder(static_cast<int32>(EPropertyColumnOrder::ConflictWarning)),
			SHeaderRow::Column(ConflictWarningPropertyColumnId)
				.DefaultLabel(FText::GetEmpty())
				.FixedWidth(20.f)
			);
	}
}

#undef LOCTEXT_NAMESPACE