// Copyright Epic Games, Inc. All Rights Reserved.


#include "ArchetypeFixupDetailCustomization.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "ArchetypeFixupPanel.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Input/SComboButton.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#define LOCTEXT_NAMESPACE "ArchetypeFixupDetails"


TSet<FPropertyPath> FArchetypeFixupDetailNodeBuilder::GetRedirectOptions(const UStruct* Struct, void* Value) const
{
	TSet<FPropertyPath> Result;
	GetRedirectOptions(Struct, Value, {}, Result);
	return Result;
}

void FArchetypeFixupDetailNodeBuilder::GetRedirectOptions(const UStruct* Struct, void* Value, const FPropertyPath& Path, TSet<FPropertyPath>& OutPaths) const
{
	for (FProperty* SubProperty : TFieldRange<FProperty>(Struct))
    {
		if (SubProperty->ArrayDim == 1)
		{
			const FPropertyPath SubPath = Path.ExtendPath(FPropertyInfo(SubProperty)).Get();
			GetRedirectOptions(SubProperty, SubProperty->ContainerPtrToValuePtr<void>(Value), SubPath, OutPaths);
		}
		else
		{
			for (int32 ArrayIndex = 0; ArrayIndex < SubProperty->ArrayDim; ++ArrayIndex)
			{
    			const FPropertyPath SubPath = Path.ExtendPath(FPropertyInfo(SubProperty, ArrayIndex)).Get();
    			GetRedirectOptions(SubProperty, SubProperty->ContainerPtrToValuePtr<void>(Value, ArrayIndex), SubPath, OutPaths);
			}
		}
    }
}

void FArchetypeFixupDetailNodeBuilder::GetRedirectOptions(const FProperty* Property, void* Value, const FPropertyPath& Path, TSet<FPropertyPath>& OutPaths) const
{
	if (Property->GetBoolMetaData(TEXT("isLoose")))
	{
		// don't include loose properties as options
		return;
	}

	if (!DiffPanel.Pin()->RedirectedPropertyTree->Find(Path))
	{
		OutPaths.Add(Path);
	}
	
	if (const FStructProperty* AsStructProperty = CastField<FStructProperty>(Property))
	{
		GetRedirectOptions(AsStructProperty->Struct, Value, Path, OutPaths);
	}
	else if (const FArrayProperty* AsArrayProperty = CastField<FArrayProperty>(Property))
	{
		FScriptArrayHelper Array(AsArrayProperty, Value);
		for (int32 ArrayIndex = 0; ArrayIndex < Array.Num(); ++ArrayIndex)
		{
			const FPropertyPath SubPath = Path.ExtendPath(FPropertyInfo(AsArrayProperty->Inner, ArrayIndex)).Get();
			GetRedirectOptions(AsArrayProperty->Inner, Array.GetElementPtr(ArrayIndex), SubPath, OutPaths);
		}
	}
	else if (const FSetProperty* AsSetProperty = CastField<FSetProperty>(Property))
	{
		FScriptSetHelper Set(AsSetProperty, Value);
		for (FScriptSetHelper::FIterator Iterator = Set.CreateIterator(); Iterator; ++Iterator)
		{
			const FPropertyPath SubPath = Path.ExtendPath(FPropertyInfo(AsSetProperty->ElementProp, Iterator.GetLogicalIndex())).Get();
			GetRedirectOptions(AsSetProperty->ElementProp, Set.GetElementPtr(Iterator), SubPath, OutPaths);
		}
	}
	else if (const FMapProperty* AsMapProperty = CastField<FMapProperty>(Property))
	{
		FScriptMapHelper Map(AsMapProperty, Value);
		for (FScriptMapHelper::FIterator Iterator = Map.CreateIterator(); Iterator; ++Iterator)
		{
			const FPropertyPath SubPath = Path.ExtendPath(FPropertyInfo(AsMapProperty->ValueProp, Iterator.GetLogicalIndex())).Get();
			GetRedirectOptions(AsMapProperty->ValueProp, Map.GetValuePtr(Iterator), SubPath, OutPaths);
		}
	}
}

/////////////////////////////////////////////////////////////////
// FArchetypeFixupDetailCustomization
/////////////////////////////////////////////////////////////////

FArchetypeFixupDetailCustomization::FArchetypeFixupDetailCustomization(const TSharedRef<FArchetypeFixupPanel>& InDiffPanel)
	: DiffPanel(InDiffPanel)
{
	
}

FArchetypeFixupDetailCustomization::~FArchetypeFixupDetailCustomization()
{
}

void FArchetypeFixupDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	const TSharedPtr<FArchetypeFixupPanel> Panel = DiffPanel.Pin();
	if (!Panel)
	{
		return;
	}
	
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	if (Objects.IsEmpty())
	{
		return;
	}
	TArray<FName> CategoryNames;
	DetailBuilder.GetCategoryNames(CategoryNames);
	for (const FName CategoryName : CategoryNames)
	{
		IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(CategoryName);
		TArray<TSharedRef<IPropertyHandle>> NonAdvancedProperties;
		Category.GetDefaultProperties(NonAdvancedProperties, true, false);
		for (const TSharedRef<IPropertyHandle>& Handle : NonAdvancedProperties)
		{
			DetailBuilder.HideProperty(Handle);
			Category.AddCustomBuilder(MakeShared<FArchetypeFixupDetailNodeBuilder>(Panel.ToSharedRef(), Handle), false);
		}
		TArray<TSharedRef<IPropertyHandle>> AdvancedProperties;
		Category.GetDefaultProperties(AdvancedProperties, false, true);
		for (const TSharedRef<IPropertyHandle>& Handle : AdvancedProperties)
		{
			DetailBuilder.HideProperty(Handle);
			Category.AddCustomBuilder(MakeShared<FArchetypeFixupDetailNodeBuilder>(Panel.ToSharedRef(), Handle), true);
		}
	}
}

/////////////////////////////////////////////////////////////////
// FHideLoosePropertiesCustomization
/////////////////////////////////////////////////////////////////

FHideLoosePropertiesCustomization::~FHideLoosePropertiesCustomization()
{
}

void FHideLoosePropertiesCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	if (Objects.IsEmpty())
	{
		return;
	}
	TArray<FName> CategoryNames;
	DetailBuilder.GetCategoryNames(CategoryNames);
	for (const FName CategoryName : CategoryNames)
	{
		IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(CategoryName);
		TArray<TSharedRef<IPropertyHandle>> Handles;
		Category.GetDefaultProperties(Handles, true, true);
		for (const TSharedRef<IPropertyHandle>& Handle : Handles)
		{
			CustomizeHandle(Handle, DetailBuilder);
		}
	}
}

void FHideLoosePropertiesCustomization::CustomizeHandle(const TSharedRef<IPropertyHandle>& Handle, IDetailLayoutBuilder& DetailBuilder)
{
	if (Handle->GetBoolMetaData(TEXT("isLoose")))
	{
		DetailBuilder.HideProperty(Handle);
	}
	else
	{
		// recurse into children
		uint32 NumChildren = 0;
		Handle->GetNumChildren(NumChildren);
		for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
		{
			CustomizeHandle(Handle->GetChildHandle(ChildIndex).ToSharedRef(), DetailBuilder);
		}
	}
}


/////////////////////////////////////////////////////////////////
// FArchetypeFixupDetailNodeBuilder
/////////////////////////////////////////////////////////////////

FArchetypeFixupDetailNodeBuilder::FArchetypeFixupDetailNodeBuilder(const TSharedRef<FArchetypeFixupPanel>& InDiffPanel, const TSharedRef<IPropertyHandle>& InPropertyHandle)
	: DiffPanel(InDiffPanel)
	, PropertyHandle(InPropertyHandle)
{}

void FArchetypeFixupDetailNodeBuilder::GenerateHeaderRowContent(FDetailWidgetRow& NodeRow)
{	
	const TSharedPtr<FArchetypeFixupPanel> Panel = DiffPanel.Pin();
	if (!Panel)
	{
		return;
	}

	// skip properties that are hidden from this view mode
	if (IsHidden())
	{
		return;
	}
	
	TSharedPtr<SWidget> InnerNameContent = PropertyHandle->CreatePropertyNameWidget();
	if (PropertyHandle->GetKeyHandle())
	{
		// TODO: @jordan.hoffmann this solution for keys only works with basic types. figure out a more robust solution.
		InnerNameContent = PropertyHandle->GetKeyHandle()->CreatePropertyValueWidget();
		InnerNameContent->SetEnabled(false);
	}

	const TSharedRef<SWidget> NameContent = SNew(SWidgetSwitcher)
		.WidgetIndex(this, &FArchetypeFixupDetailNodeBuilder::GetNameWidgetIndex)
		+SWidgetSwitcher::Slot()
		[
			InnerNameContent.ToSharedRef()
		]
		+SWidgetSwitcher::Slot()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.VAlign(EVerticalAlignment::VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Visibility(this, &FArchetypeFixupDetailNodeBuilder::DeletionSymbolVisibility)
				.Text(LOCTEXT("MarkedForDeletion", "❌"))
			]
			+SHorizontalBox::Slot()
			.VAlign(EVerticalAlignment::VAlign_Center)
			.AutoWidth()
			[
				SNew(SComboButton)
				.Visibility(EVisibility::Visible)
				.OnGetMenuContent_Raw(this, &FArchetypeFixupDetailNodeBuilder::GeneratePropertyRedirectMenu)
				.ButtonContent()
				[
					InnerNameContent.ToSharedRef()
				]
			]
		];
	
	const TSharedRef<SWidget> ValueContent = PropertyHandle->CreatePropertyValueWidget();
	ValueContent->SetEnabled(!Panel->HasViewFlag(FArchetypeFixupPanel::EViewFlags::ReadonlyValues));
	ValueContent->SetVisibility(TAttribute<EVisibility>(this, &FArchetypeFixupDetailNodeBuilder::ValueContentVisibility));
	
	NodeRow
	.NameContent()
	[
		NameContent
	]
	.ValueContent()
	[
		ValueContent
	]
	.PropertyHandleList({PropertyHandle});
}

void FArchetypeFixupDetailNodeBuilder::GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder)
{
	const TSharedPtr<FArchetypeFixupPanel> Panel = DiffPanel.Pin();
	if (!Panel)
	{
		return;
	}

	// skip properties that are hidden from this view mode
	if (IsHidden())
	{
		return;
	}
	
	uint32 ChildCount;
	PropertyHandle->GetNumChildren(ChildCount);
	for (uint32 I = 0; I < ChildCount; ++I)
	{
		const TSharedRef<IPropertyHandle> ChildHandle = PropertyHandle->GetChildHandle(I).ToSharedRef();
		ChildrenBuilder.AddCustomBuilder(MakeShared<FArchetypeFixupDetailNodeBuilder>(Panel.ToSharedRef(), ChildHandle));
	}
}

FName FArchetypeFixupDetailNodeBuilder::GetName() const
{
	return PropertyHandle->GetProperty()->GetFName();
}

TSharedPtr<IPropertyHandle> FArchetypeFixupDetailNodeBuilder::GetPropertyHandle() const
{
	return PropertyHandle;
}

int32 FArchetypeFixupDetailNodeBuilder::GetNameWidgetIndex() const
{
	if (const TSharedPtr<FArchetypeFixupPanel> Panel = DiffPanel.Pin())
	{
		if (Panel->HasViewFlag(FArchetypeFixupPanel::EViewFlags::AllowRemapLooseProperties))
		{
			if (Panel->RevertInfo.Contains(*PropertyHandle->CreateFPropertyPath()))
			{
				return DisplayRedirectMenu;
			}
			if (const FProperty* Property = PropertyHandle->GetProperty())
			{
				if (Property->GetBoolMetaData(TEXT("isLoose")))
				{
					return DisplayRedirectMenu;
				}
			}
		}
	}
	return DisplayRegularName;
}

TSharedRef<SWidget> FArchetypeFixupDetailNodeBuilder::GeneratePropertyRedirectMenu() const
{
	FMenuBuilder MenuBuilder(true, nullptr);
	
	const TSharedPtr<FArchetypeFixupPanel> Panel = DiffPanel.Pin();
	if (!Panel)
	{
		return MenuBuilder.MakeWidget();
	}
	const FPropertyPath Path = *PropertyHandle->CreateFPropertyPath();
	const FPropertyPath& OriginalPath = Panel->GetOriginalPath(Path);

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("ResetRedirect", "Reset"));
	if (OriginalPath != Path || Panel->MarkedForDelete.Contains(OriginalPath))
	{
		FText OriginalPathText = FText::FromString(OriginalPath.ToString());
		FText Tooltip = FText::Format(LOCTEXT("ResetTooltip", "Reset back to {0}"), OriginalPathText);
		MenuBuilder.AddMenuEntry(OriginalPathText, Tooltip, FSlateIcon()
						, FUIAction(FExecuteAction::CreateSP(Panel.Get(), &FArchetypeFixupPanel::OnRedirectProperty, Path, OriginalPath))
						, NAME_None
						, EUserInterfaceActionType::RadioButton);
	}
	MenuBuilder.EndSection();

	UObject* FirstArchetype = Panel->Instances[0];
	TSet<FPropertyPath> RedirectOptions = GetRedirectOptions(FirstArchetype->GetClass(), FirstArchetype);

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("MoveProperty", "Move"));
	{
		for (const FPropertyPath& Option : RedirectOptions)
		{
			FName ThisPropName = PropertyHandle->GetProperty()->GetFName();
			FName OptionPropName = Option.GetLeafMostProperty().Property->GetFName();
			if (ThisPropName == OptionPropName)
			{
				if (Option.GetLeafMostProperty().Property->SameType(PropertyHandle->GetProperty()))
				{
					FText DisplayName = FText::FromString(Option.ToString());
					FText Tooltip = FText::Format(LOCTEXT("MovePropertyTooltip", "Move property to '{0}'"), DisplayName);
					MenuBuilder.AddMenuEntry(DisplayName, Tooltip, FSlateIcon()
					, FUIAction(FExecuteAction::CreateSP(Panel.Get(), &FArchetypeFixupPanel::OnRedirectProperty, Path, Option))
					, NAME_None
					, EUserInterfaceActionType::RadioButton);
				}
			}
		}
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("RenameProperty", "Rename"));
	{
		for (const FPropertyPath& Option : RedirectOptions)
		{
			FName ThisPropName = PropertyHandle->GetProperty()->GetFName();
			FName OptionPropName = Option.GetLeafMostProperty().Property->GetFName();
			if (ThisPropName == OptionPropName)
			{
				continue; // handled in the "move" category
			}
			if (Option.GetLeafMostProperty().Property->SameType(PropertyHandle->GetProperty()))
			{
				FText DisplayName = FText::FromString(Option.ToString());
				FText Tooltip = FText::Format(LOCTEXT("RenamePropertyTooltip", "Rename property to '{0}'"), DisplayName);
				MenuBuilder.AddMenuEntry(DisplayName, Tooltip, FSlateIcon()
				, FUIAction(FExecuteAction::CreateSP(Panel.Get(), &FArchetypeFixupPanel::OnRedirectProperty, Path, Option))
				, NAME_None
				, EUserInterfaceActionType::RadioButton);
			}
		}
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("ChangeToDifferingTypeConversion", "Convert Type (potential data loss)"));
	{
		// TODO: Add type conversions?
	}
	MenuBuilder.EndSection();
	
	MenuBuilder.BeginSection(NAME_None, LOCTEXT("Delete", "Delete"));
	{
		if (!Panel->MarkedForDelete.Contains(OriginalPath)) // check that it wasn't already marked as deleted
		{
			FText DisplayName = LOCTEXT("MarkForDeletion", "Mark For Deletion");
			FText Tooltip = LOCTEXT("MarkForDeletionTooltip", "Mark this property for deletion");
			MenuBuilder.AddMenuEntry(DisplayName, Tooltip, FSlateIcon()
						, FUIAction(FExecuteAction::CreateSP(Panel.Get(), &FArchetypeFixupPanel::OnMarkForDelete, Path))
						, NAME_None
						, EUserInterfaceActionType::RadioButton);
		}
	}
	MenuBuilder.EndSection();
	
	return MenuBuilder.MakeWidget();
}

EVisibility FArchetypeFixupDetailNodeBuilder::DeletionSymbolVisibility() const
{
	if (const TSharedPtr<FArchetypeFixupPanel> Panel = DiffPanel.Pin())
	{
		const FPropertyPath Path = *PropertyHandle->CreateFPropertyPath();
		return Panel->MarkedForDelete.Contains(Path) ? EVisibility::Visible :  EVisibility::Collapsed;
	}
	return EVisibility::Collapsed;
}

EVisibility FArchetypeFixupDetailNodeBuilder::ValueContentVisibility() const
{
	if (const TSharedPtr<FArchetypeFixupPanel> Panel = DiffPanel.Pin())
	{
		const FPropertyPath Path = *PropertyHandle->CreateFPropertyPath();
		return Panel->MarkedForDelete.Contains(Path) ? EVisibility::Collapsed :  EVisibility::Visible;
	}
	return EVisibility::Visible;
}

bool FArchetypeFixupDetailNodeBuilder::IsHidden() const
{
	const TSharedPtr<FArchetypeFixupPanel> Panel = DiffPanel.Pin();
	if (!Panel)
	{
		return true;
	}

	if (const FProperty* Property = PropertyHandle->GetProperty())
	{
		if (Panel->HasViewFlag(FArchetypeFixupPanel::EViewFlags::HideLooseProperties) &&
			Property->GetBoolMetaData(TEXT("isLoose")))
		{
			return true;
		}

		if (Panel->HasViewFlag(FArchetypeFixupPanel::EViewFlags::IncludeOnlySetBySerialization))
		{
			if (!Panel->IsInRedirectedPropertyTree(*PropertyHandle->CreateFPropertyPath()))
			{
				return true;
			}
		}
	}
	return false;
}

#undef LOCTEXT_NAMESPACE
