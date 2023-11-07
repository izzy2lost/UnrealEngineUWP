// Copyright Epic Games, Inc. All Rights Reserved.

#include "SingleClientColumns.h"

#include "IConcertClient.h"
#include "SOwnerClientList.h"
#include "Replication/Editor/View/IReplicationStreamViewer.h"
#include "Replication/Editor/View/ReplicationColumnsUtils.h"
#include "Replication/Util/GlobalAuthorityCache.h"
#include "Widgets/ActiveSession/Replication/Client/ClientUtils.h"
#include "Widgets/ClientName/SClientName.h"

#define LOCTEXT_NAMESPACE "SingleClientColumns.Owner"

namespace UE::MultiUserClient::SingleClientColumns
{
	const FName OwnerOfSubobjectColumnId = TEXT("OwnerOfSubobjectColumn");
	const FName OwnerOfPropertyColumnId = TEXT("OwnerOfPropertyColumn");

	namespace Private::Shared
	{
		constexpr float DefaultWidth = 200.f;
		
		static void PopulateSearchTerms(const TSharedRef<IConcertClient>& InClient, const TArray<FGuid>& Clients, TArray<FString>& InOutSearchStrings)
		{
			for (const FGuid& ClientId : Clients)
			{
				InOutSearchStrings.Add(ClientUtils::GetClientDisplayName(*InClient, ClientId));
			}
		}
	}

	namespace Private::TopLevel
	{
		static TArray<FGuid> GetOwnersOfTopLevelObject(
			const FGlobalAuthorityCache& InAuthorityCache,
			ConcertClientSharedSlate::IObjectToPropertiesModel& InObjectModel,
			const FSoftObjectPath& InTopLevelObject
			)
		{
			TArray<FGuid> Owners = InAuthorityCache.GetClientsWithAuthorityOverObject(InTopLevelObject);
			InObjectModel.ForEachSubobject(InTopLevelObject, [&InAuthorityCache, &Owners](const FSoftObjectPath& Subobject)
			{
				for (const FGuid& Client : InAuthorityCache.GetClientsWithAuthorityOverObject(Subobject))
				{
					Owners.AddUnique(Client);
				}
				return EBreakBehavior::Continue;
			});
			return Owners;
		}
	}
	
	ConcertClientSharedSlate::ReplicationColumns::FReplicationTopLevelObjectColumn OwnerOfTopLevelObject(
		const TSharedRef<IConcertClient>& InClient,
		FGlobalAuthorityCache& InAuthorityCache,
		ConcertClientSharedSlate::IObjectToPropertiesModel& InObjectModel
		)
	{
		using FColumnType = ConcertClientSharedSlate::ReplicationColumns::FReplicationSubobjectObjectColumn;
		return FColumnType(
			FColumnType::FArguments()
				.GenerateWidgetColumn_Lambda([InClient, &InObjectModel, &InAuthorityCache](const FColumnType::FBuildArgs& InArgs)
				{
					return SNew(SOwnerClientList, InClient, InAuthorityCache)
						.GetClientList_Lambda([&InAuthorityCache, &InObjectModel, ObjectPath = InArgs.RowData.GetObjectPath()](const FGlobalAuthorityCache&)
						{
							return Private::TopLevel::GetOwnersOfTopLevelObject(InAuthorityCache, InObjectModel, ObjectPath);
						})
						.HighlightText_Lambda([HighlightText = InArgs.HighlightText](){ return *HighlightText.Get(); });
				})
				.PopulateSearchItems_Lambda([InClient, &InAuthorityCache, &InObjectModel](const ConcertClientSharedSlate::FReplicatedObjectData& InArgs, TArray<FString>& InOutSearchStrings)
				{
					Private::Shared::PopulateSearchTerms(
						InClient,
						Private::TopLevel::GetOwnersOfTopLevelObject(InAuthorityCache, InObjectModel, InArgs.GetObjectPath()),
						InOutSearchStrings
						);
				})
				.ColumnSortOrder(static_cast<int32>(ETopLevelObjectColumnOrder::Owner)),
			SHeaderRow::Column(OwnerOfSubobjectColumnId)
				.DefaultLabel(LOCTEXT("Subobject.Owner", "Owner"))
				.FillSized(Private::Shared::DefaultWidth)
			);
	}
	
	ConcertClientSharedSlate::ReplicationColumns::FReplicationSubobjectObjectColumn OwnerOfSubobject(
		const TSharedRef<IConcertClient>& InClient,
		FGlobalAuthorityCache& InAuthorityCache
		)
	{
		using FColumnType = ConcertClientSharedSlate::ReplicationColumns::FReplicationSubobjectObjectColumn;
		return FColumnType(
			FColumnType::FArguments()
				.GenerateWidgetColumn_Lambda([InClient, &InAuthorityCache](const FColumnType::FBuildArgs& InArgs)
				{
					return SNew(SOwnerClientList, InClient, InAuthorityCache)
						.GetClientList_Lambda([&InAuthorityCache, ObjectPath = InArgs.RowData.GetObjectPath()](const FGlobalAuthorityCache&)
						{
							return InAuthorityCache.GetClientsWithAuthorityOverObject(ObjectPath);
						})
						.HighlightText_Lambda([HighlightText = InArgs.HighlightText](){ return *HighlightText.Get(); });
				})
				.PopulateSearchItems_Lambda([InClient, &InAuthorityCache](const ConcertClientSharedSlate::FReplicatedObjectData& InArgs, TArray<FString>& InOutSearchStrings)
				{
					Private::Shared::PopulateSearchTerms(
						InClient,
						InAuthorityCache.GetClientsWithAuthorityOverObject(InArgs.GetObjectPath()),
						InOutSearchStrings
						);
				})
				.ColumnSortOrder(static_cast<int32>(ESubobjectColumnOrder::Owner)),
			SHeaderRow::Column(OwnerOfSubobjectColumnId)
				.DefaultLabel(LOCTEXT("Subobject.Owner", "Owner"))
				.FillSized(Private::Shared::DefaultWidth)
			);
	}
	
	namespace Private::Property
	{
		static TArray<FGuid> GetPropertyOwners(
			const FGlobalAuthorityCache& InAuthorityCache,
			const ConcertClientSharedSlate::IReplicationStreamViewer& InViewer,
			const FConcertPropertyChain& Property
			)
		{
			TArray<FGuid> Result;
			const TArray<FSoftObjectPath> SelectedObjects = InViewer.GetObjectsBeingPropertyEdited();
			for (const FSoftObjectPath& SelectedObject : SelectedObjects)
			{
				const TOptional<FGuid> Owner = InAuthorityCache.GetClientWithAuthorityOverProperty(SelectedObject, Property);
				if (Owner)
				{
					Result.AddUnique(*Owner);
				}
			}
			return Result;
		}
	}

	ConcertClientSharedSlate::ReplicationColumns::FReplicationPropertyColumn OwnerOfProperty(
		const TSharedRef<IConcertClient>& InClient,
		FGlobalAuthorityCache& InAuthorityCache,
		const TAttribute<const ConcertClientSharedSlate::IReplicationStreamViewer*>& InViewer
		)
	{
		using FColumnType = ConcertClientSharedSlate::ReplicationColumns::FReplicationPropertyColumn;
		return FColumnType(
			FColumnType::FArguments()
				.GenerateWidgetColumn_Lambda([InClient, &InAuthorityCache, InViewer](const FColumnType::FBuildArgs& InArgs)
				{
					return SNew(SOwnerClientList, InClient, InAuthorityCache)
						.GetClientList_Lambda([&InAuthorityCache, InViewer, Property = InArgs.RowData.GetProperty()](const FGlobalAuthorityCache&)
						{
							return Private::Property::GetPropertyOwners(InAuthorityCache, *InViewer.Get(), Property);
						})
						.HighlightText_Lambda([HighlightText = InArgs.HighlightText](){ return *HighlightText.Get(); });
				})
				.PopulateSearchItems_Lambda([InClient, &InAuthorityCache, InViewer](const ConcertClientSharedSlate::FReplicatedPropertyData& InArgs, TArray<FString>& InOutSearchStrings)
				{
					Private::Shared::PopulateSearchTerms(
						InClient,
						Private::Property::GetPropertyOwners(InAuthorityCache, *InViewer.Get(), InArgs.GetProperty()),
						InOutSearchStrings
						);
				})
				.ColumnSortOrder(static_cast<int32>(EPropertyColumnOrder::Owner)),
			SHeaderRow::Column(OwnerOfPropertyColumnId)
				.DefaultLabel(LOCTEXT("Property.Owner", "Owner"))
				.FillSized(Private::Shared::DefaultWidth)
			);
	}
}

#undef LOCTEXT_NAMESPACE