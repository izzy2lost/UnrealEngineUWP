// Copyright Epic Games, Inc. All Rights Reserved.

#include "SRCPanelExposedEntitiesGroup.h"

#include "RemoteControlPreset.h"
#include "SRCPanelExposedField.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SEditableTextBox.h"

#define LOCTEXT_NAMESPACE "SRCPanelExposedEntitiesGroup"

void SRCPanelExposedEntitiesGroup::Construct(const FArguments& InArgs, EFieldGroupType InFieldGroupType, URemoteControlPreset* Preset)
{
	FieldKey = InArgs._FieldKey;
	GroupType = InFieldGroupType;
	PresetWeak = Preset;
	OnGroupPropertyIdChangedDelegate = InArgs._OnGroupPropertyIdChanged;

	const FMakeNodeWidgetArgs Args = CreateNodeWidgetArgs();
	MakeNodeWidget(Args);
}

SRCPanelTreeNode::FMakeNodeWidgetArgs SRCPanelExposedEntitiesGroup::CreateNodeWidgetArgs()
{
	FMakeNodeWidgetArgs Args;

	Args.PropertyIdWidget = SNew(SBox)
		[
			SNew(SEditableTextBox)
			.Justification(ETextJustify::Left)
			.MinDesiredWidth(50.f)
			.SelectAllTextWhenFocused(true)
			.RevertTextOnEscape(true)
			.ClearKeyboardFocusOnCommit(true)
			.Text_Lambda([this]{ return FText::FromName(PropertyIdName); })
			.OnTextCommitted(this, &SRCPanelExposedEntitiesGroup::OnPropertyIdTextCommitted)
		];

	Args.OwnerNameWidget = SNew(SBox)
		.HeightOverride(25)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text_Lambda([this]{ return FText::FromName(OwnerName); })
		];

	FText GroupText;

	switch (GroupType)
	{
	case EFieldGroupType::PropertyId:
		GroupText = LOCTEXT("GroupPropertyId", "Group by Id");
		break;

	case EFieldGroupType::Owner:
		GroupText = LOCTEXT("GroupOwner", "Group by Owner");
		break;
	}

	Args.NameWidget = SNew(SBox)
		.HeightOverride(25)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(GroupText)
		];

	Args.SubObjectPathWidget = SNullWidget::NullWidget;
	Args.ValueWidget = SNullWidget::NullWidget;
	Args.ResetButton = SNullWidget::NullWidget;

	return Args;
}

void SRCPanelExposedEntitiesGroup::OnPropertyIdTextCommitted(const FText& InText, ETextCommit::Type InTextCommitType)
{
	if (!PresetWeak.IsValid() || PresetWeak.IsStale())
	{
		return;
	}

	const FName NewId = FName(InText.ToString());

	if (FieldKey.Compare(NewId) == 0)
	{
		return;
	}

	URemoteControlPreset* Preset = PresetWeak.Get();
	FieldKey = NewId;

	for (const TSharedPtr<SRCPanelTreeNode>& Child : ChildWidgets)
	{
		TWeakPtr<FRemoteControlField> ExposedField = Preset->GetExposedEntity<FRemoteControlField>(Child->GetRCId());
		if (ExposedField.IsValid())
		{
			ExposedField.Pin()->PropertyId = NewId;
			Child->SetPropertyId(NewId);
			Preset->UpdateIdentifiedField(ExposedField.Pin().ToSharedRef());
		}
	}

	OnGroupPropertyIdChangedDelegate.ExecuteIfBound();
}

void SRCPanelExposedEntitiesGroup::AssignChildren(const TArray<TSharedPtr<SRCPanelTreeNode>>& InFieldEntities)
{
	ChildWidgets.Empty();

	OwnerName = NAME_None;
	PropertyIdName = NAME_None;

	auto CompareUpdateName = [](FName& InOutName, const FName& InOtherName)
		{
			if (InOutName == NAME_None)
			{
				InOutName = InOtherName;
			}
			else if (InOutName != InOtherName)
			{
				InOutName = TEXT("Multiple Values");
			}
		};

	for (const TSharedPtr<SRCPanelTreeNode>& Entity : InFieldEntities)
	{
		if (const TSharedPtr<SRCPanelExposedField> ExposedField = StaticCastSharedPtr<SRCPanelExposedField>(Entity))
		{
			const FName FieldOwnerName  = ExposedField->GetOwnerName();
			const FName FieldPropertyId = ExposedField->GetPropertyId();

			const bool bOwnerMatch = GroupType == EFieldGroupType::Owner && FieldOwnerName == FieldKey;
			const bool bPropertyIdMatch = GroupType == EFieldGroupType::PropertyId && FieldPropertyId == FieldKey;

			if (bOwnerMatch || bPropertyIdMatch)
			{
				ChildWidgets.Add(Entity);
				CompareUpdateName(OwnerName, FieldOwnerName);
				CompareUpdateName(PropertyIdName, FieldPropertyId);
			}
		}
	}
}

void SRCPanelExposedEntitiesGroup::GetNodeChildren(TArray<TSharedPtr<SRCPanelTreeNode>>& OutChildren) const
{
	OutChildren.Append(ChildWidgets);
	SRCPanelTreeNode::GetNodeChildren(OutChildren);
}

#undef LOCTEXT_NAMESPACE
