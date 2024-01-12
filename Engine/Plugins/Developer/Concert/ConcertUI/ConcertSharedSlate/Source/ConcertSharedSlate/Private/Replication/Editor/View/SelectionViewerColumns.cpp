// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/Editor/View/SelectionViewerColumns.h"

#include "Replication/Editor/Model/IEditableReplicationStreamModel.h"
#include "Replication/Editor/Model/Object/IObjectNameModel.h"
#include "Replication/Editor/Model/ReplicatedPropertyData.h"
#include "Replication/Editor/Model/ReplicatedObjectData.h"
#include "Replication/Editor/View/DisplayUtils.h"
#include "Replication/Editor/View/ObjectEditor/SBaseReplicationStreamEditor.h"
#include "Replication/Editor/View/ReplicationColumnsUtils.h"
#include "Replication/ObjectUtils.h"
#include "Replication/PropertyChainUtils.h"

#include "Internationalization/Internationalization.h"
#include "Textures/SlateIcon.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SCheckBox.h"

#define LOCTEXT_NAMESPACE "ReplicationObjectColumns"

namespace UE::ConcertSharedSlate::ReplicationColumns::TopLevel
{
	const FName IconColumnId = TEXT("IconColumn");
	const FName LabelColumnId = TEXT("LabelColumn");
	const FName TypeColumnId = TEXT("TypeColumn");
	
	FReplicationTopLevelObjectColumn LabelColumn(TSharedRef<IReplicationStreamModel> Model, IObjectNameModel* OptionalNameModel)
	{
		auto GetDisplayText = [OptionalNameModel](const FReplicatedObjectData& ObjectData)
		{
			const FSoftObjectPath& ObjectPath = ObjectData.GetObjectPath();
			return DisplayUtils::GetObjectDisplayText(ObjectPath, OptionalNameModel);
		};
		
		return FReplicationTopLevelObjectColumn(
			FReplicationTopLevelObjectColumn::FArguments()
				.GenerateWidgetColumn_Lambda([Model = MoveTemp(Model), GetDisplayText](const FReplicationTopLevelObjectColumn::FBuildArgs& Args)
				{
					const FText Text = GetDisplayText(Args.RowData);
					
					return SNew(SHorizontalBox)
						.ToolTipText(FText::FromString(Args.RowData.GetObjectPath().ToString()))
					
						+SHorizontalBox::Slot()
						.AutoWidth()
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(SImage)
							.Image(DisplayUtils::GetObjectIcon(*Model, Args.RowData.GetObjectPath()).GetOptionalIcon())
						]
					
						+SHorizontalBox::Slot()
						.HAlign(HAlign_Left)
						.VAlign(VAlign_Center)
						.Padding(6.f, 0.f, 0.f, 0.f)
						[
							SNew(STextBlock)
							.HighlightText(TAttribute<FText>::CreateLambda([HighlightText = Args.HighlightText](){ return *HighlightText; }))
							.Text(Text)
						];
				})
				.PopulateSearchItems_Lambda([OptionalNameModel](const FReplicatedObjectData& ObjectData, TArray<FString>& InOutSearchStrings)
				{
					InOutSearchStrings.Add(DisplayUtils::GetObjectDisplayText(ObjectData.GetObjectPath(), OptionalNameModel).ToString());
				})
				.IsLessThan_Lambda([GetDisplayText](const FReplicatedObjectData& Left, const FReplicatedObjectData& Right)
				{
					return GetDisplayText(Left).ToString() < GetDisplayText(Right).ToString();
				})
				.ColumnSortOrder(static_cast<int32>(ETopLevelColumnOrder::Label)),
			SHeaderRow::Column(LabelColumnId)
				.DefaultLabel(LOCTEXT("LabelColumnLabel", "Label"))
				.FillWidth(1.f)
			);
	}
	
	FReplicationTopLevelObjectColumn TypeColumn(TSharedRef<IReplicationStreamModel> Model)
	{
		return FReplicationTopLevelObjectColumn(
			FReplicationTopLevelObjectColumn::FArguments()
				.GenerateWidgetColumn_Lambda([Model](const FReplicationTopLevelObjectColumn::FBuildArgs& Args)
				{
					return SNew(STextBlock)
						.HighlightText(TAttribute<FText>::CreateLambda([HighlightText = Args.HighlightText](){ return *HighlightText; }))
						.Text(DisplayUtils::GetObjectTypeText(*Model, Args.RowData.GetObjectPath()));
				})
				.PopulateSearchItems_Lambda([Model](const FReplicatedObjectData& ObjectData, TArray<FString>& InOutSearchStrings)
				{
					InOutSearchStrings.Add(DisplayUtils::GetObjectTypeText(Model.Get(), ObjectData.GetObjectPath()).ToString());
				})
				.IsLessThan_Lambda([Model](const FReplicatedObjectData& Left, const FReplicatedObjectData& Right)
				{
					return DisplayUtils::GetObjectTypeText(*Model, Left.GetObjectPath()).ToString() < DisplayUtils::GetObjectTypeText(*Model, Right.GetObjectPath()).ToString();
				})
				.ColumnSortOrder(static_cast<int32>(ETopLevelColumnOrder::Type)),
			SHeaderRow::Column(TypeColumnId)
				.DefaultLabel(LOCTEXT("TypeColumnLabel", "Type"))
				.FillWidth(1.f)
			);
	}
}

#undef LOCTEXT_NAMESPACE
#define LOCTEXT_NAMESPACE "ReplicationPropertyColumns"

namespace UE::ConcertSharedSlate::ReplicationColumns::Property
{
	const FName ReplicatesColumnId = TEXT("ReplicatedColumn");
	const FName LabelColumnId = TEXT("LabelColumn");
	const FName TypeColumnId = TEXT("TypeColumn");
	
	FReplicationPropertyColumn LabelColumn()
	{
		return FReplicationPropertyColumn(
			FReplicationPropertyColumn::FArguments()
				.GenerateWidgetColumn_Lambda([](const FReplicationPropertyColumn::FBuildArgs& Args)
				{
					return SNew(STextBlock)
						.HighlightText(TAttribute<FText>::CreateLambda([HighlightText = Args.HighlightText](){ return *HighlightText; }))
						.Text(DisplayUtils::GetPropertyDisplayText(Args.RowData.GetProperty()));
				})
				.PopulateSearchItems_Lambda([](const FReplicatedPropertyData& ObjectData, TArray<FString>& InOutSearchStrings)
				{
					InOutSearchStrings.Add(DisplayUtils::GetPropertyDisplayText(ObjectData.GetProperty()).ToString());
				})
				.IsLessThan_Lambda([](const FReplicatedPropertyData& Left, const FReplicatedPropertyData& Right)
				{
					return DisplayUtils::GetPropertyDisplayString(Left.GetProperty()) < DisplayUtils::GetPropertyDisplayString(Right.GetProperty());
				})
				.ColumnSortOrder(static_cast<int32>(EReplicationPropertyColumnOrder::Label)),
			SHeaderRow::Column(LabelColumnId)
				.DefaultLabel(LOCTEXT("LabelColumnLabel", "Label"))
				.FillWidth(1.f)
			);
	}
	
	FReplicationPropertyColumn TypeColumn()
	{
		static auto GetDisplayText = [](const FReplicatedPropertyData& Args)
		{
			UClass* Class = Args.GetOwningClass().TryLoadClass<UObject>();
			const FProperty* Property = Class ? ConcertSyncCore::PropertyChain::ResolveProperty(*Class, Args.GetProperty()) : nullptr;
			return Property ? FText::FromString(Property->GetCPPType()) : LOCTEXT("Unknown", "Unknown");	
		};
		
		return FReplicationPropertyColumn(
			FReplicationPropertyColumn::FArguments()
				.GenerateWidgetColumn_Lambda([](const FReplicationPropertyColumn::FBuildArgs& Args)
				{
					return SNew(STextBlock)
						.HighlightText(TAttribute<FText>::CreateLambda([HighlightText = Args.HighlightText](){ return *HighlightText; }))
						.Text(GetDisplayText(Args.RowData));
				})
				.PopulateSearchItems_Lambda([](const FReplicatedPropertyData& ObjectData, TArray<FString>& InOutSearchStrings)
				{
					InOutSearchStrings.Add(GetDisplayText(ObjectData).ToString());
				})
				.IsLessThan_Lambda([](const FReplicatedPropertyData& Left, const FReplicatedPropertyData& Right)
				{
					return GetDisplayText(Left).ToString() < GetDisplayText(Right).ToString();
				})
				.ColumnSortOrder(static_cast<int32>(EReplicationPropertyColumnOrder::Type)),
			SHeaderRow::Column(TypeColumnId)
				.DefaultLabel(LOCTEXT("TypeColumnLabel", "Type"))
				.FillWidth(0.5f)
			);
	}
}

#undef LOCTEXT_NAMESPACE