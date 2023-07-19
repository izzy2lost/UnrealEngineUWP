// Copyright Epic Games, Inc. All Rights Reserved.

#include "SParameterPicker.h"

#include "Param/AnimNextParameterBlock.h"
#include "UncookedOnlyUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Param/ParamType.h"
#include "Param/AnimNextParameter.h"
#include "Param/AnimNextParameterBlock_EditorData.h"
#include "Param/AnimNextParameterLibrary.h"
#include "DetailLayoutBuilder.h"
#include "EditorUtils.h"
#include "Param/ParameterPickerArgs.h"
#include "Widgets/Input/SSearchBox.h"
#include "String/ParseTokens.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Images/SImage.h"

#define LOCTEXT_NAMESPACE "SParameterPicker"

namespace UE::AnimNext::Editor
{

namespace ParameterPicker
{
static FName Column_Parameter(TEXT("Parameter"));
static FName Column_Library(TEXT("Library"));
static FName Column_Block(TEXT("Block"));
static FName Column_Type(TEXT("Type"));
}

struct FParameterPickerEntry
{
	enum class EFilterResult : uint8
	{
		DoesNotPassFilter	= 0x00,
		PassesFilter		= 0x01,
		ChildPassesFilter	= 0x02,
	};

	FRIEND_ENUM_CLASS_FLAGS(EFilterResult);

	FParameterPickerEntry() = default;

	FParameterPickerEntry(const FParameterBindingReference& InBinding, const FAnimNextParamType& InParamType)
		: Binding(InBinding)
		, ParamType(InParamType)
	{
		PinType = UE::AnimNext::UncookedOnly::FUtils::GetPinTypeFromParamType(ParamType);
		PinIcon = FBlueprintEditorUtils::GetIconFromPin(PinType, /* bIsLarge = */true);
		PinColor = GetDefault<UEdGraphSchema_K2>()->GetPinTypeColor(PinType);
		FString ParameterString = Binding.Parameter.ToString();
		int32 LastUnderscoreLoc = INDEX_NONE;
		if (ParameterString.FindLastChar(TEXT('_'), LastUnderscoreLoc))
		{
			ParameterString.RightChopInline(LastUnderscoreLoc + 1);
		}
		DisplayString = MoveTemp(ParameterString);
	}

	FParameterPickerEntry(FName InName)
	{
		Binding.Parameter = InName;
		FString ParameterString = Binding.Parameter.ToString();
		int32 LastUnderscoreLoc = INDEX_NONE;
		if (ParameterString.FindLastChar(TEXT('_'), LastUnderscoreLoc))
		{
			ParameterString.RightChopInline(LastUnderscoreLoc + 1);
		}
		DisplayString = MoveTemp(ParameterString);
	}

	bool PassesFilter(const FString& InFilterText) const
	{
		return Binding.Parameter.ToString().Contains(InFilterText);
	}

	FString DisplayString;

	FParameterBindingReference Binding;

	FAnimNextParamType ParamType;

	FEdGraphPinType PinType;

	const FSlateBrush* PinIcon = nullptr;

	FLinearColor PinColor = FLinearColor::White;

	TSharedPtr<FParameterPickerEntry> Parent;

	TArray<TSharedRef<FParameterPickerEntry>> Children;

	TArray<TSharedRef<FParameterPickerEntry>> FilteredChildren;

	EFilterResult FilterResult = EFilterResult::PassesFilter;
};

ENUM_CLASS_FLAGS(FParameterPickerEntry::EFilterResult);

void SParameterPicker::Construct(const FArguments& InArgs)
{
	using namespace ParameterPicker;

	Args = InArgs._Args;

	if(Args.OnGetParameterBindings != nullptr)
	{
		Args.OnGetParameterBindings->BindSP(this, &SParameterPicker::HandleGetParameterBindings);
	}

	TSharedPtr<SHeaderRow> HeaderRow;

	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.0f)
		[
			SNew(SSearchBox)
			.OnTextChanged_Lambda([this](FText InText)
			{
				FilterText = InText;
				RefreshFilter();
			})
		]
		+SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(2.0f)
		[
			SAssignNew(EntriesList, STreeView<TSharedRef<FParameterPickerEntry>>)
			.TreeItemsSource(&FilteredHierarchy)
			.SelectionMode(Args.bMultiSelect ? ESelectionMode::Multi : ESelectionMode::Single)
			.OnGenerateRow(this, &SParameterPicker::HandleGenerateRow)
			.OnSelectionChanged(this, &SParameterPicker::HandleSelectionChanged)
			.OnGetChildren(this, &SParameterPicker::HandleGetChildren)
			.OnIsSelectableOrNavigable(this, &SParameterPicker::HandleIsSelectableOrNavigable)
			.ItemHeight(20.0f)
			.HeaderRow(
				SAssignNew(HeaderRow, SHeaderRow)
				+SHeaderRow::Column(Column_Type)
				.DefaultLabel(FText::GetEmpty())
				.FixedWidth(24.0f)
				.HeaderContent()
				[
					SNew(SBox)
					.WidthOverride(16.0f)
					.HeightOverride(16.0f)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					[
						SNew(SImage)
						.ColorAndOpacity(FSlateColor::UseForeground())
						.Image(FAppStyle::GetBrush("Kismet.VariableList.TypeIcon"))
						.ToolTipText(LOCTEXT("TypeColumnHeaderTooltip", "The parameter's type"))
					]
				]

				+SHeaderRow::Column(Column_Parameter)
				.DefaultLabel(LOCTEXT("ParameterColumnHeader", "Parameter"))
				.ToolTipText(LOCTEXT("ParameterColumnHeaderTooltip", "The parameter's name"))
				.FillWidth(0.33f)
			)
		]
	];

	if (Args.bShowLibraries)
	{
		HeaderRow->AddColumn(
			SHeaderRow::Column(Column_Library)
			.DefaultLabel(LOCTEXT("LibraryColumnHeader", "Library"))
			.ToolTipText(LOCTEXT("LibraryColumnHeaderTooltip", "The library that the parameter is declared in"))
			.FillWidth(0.33f));
	}
	
	if (Args.bShowLibraries)
	{
		HeaderRow->AddColumn(
			SHeaderRow::Column(Column_Block)
			.DefaultLabel(LOCTEXT("BlockColumnHeader", "Block"))
			.ToolTipText(LOCTEXT("BlockColumnHeaderTooltip", "The parameter block that has a binding to the parameter"))
			.FillWidth(0.33f));
	}

	RefreshEntries();
}

void SParameterPicker::RefreshEntries()
{
	Entries.Empty();

	IAssetRegistry& AssetRegistry = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

	FARFilter ARFilter;

	TSet<TTuple<FName, FAssetData>> BoundParameters;

	TMap<FAssetData, FAnimNextParameterLibraryAssetRegistryExports> LibraryExportMap;

	auto GetExportsForLibrary = [&LibraryExportMap](const FAssetData& InLibrary) -> const FAnimNextParameterLibraryAssetRegistryExports&
	{
		if(FAnimNextParameterLibraryAssetRegistryExports* ExistingExports = LibraryExportMap.Find(InLibrary))
		{
			return *ExistingExports;
		}

		FAnimNextParameterLibraryAssetRegistryExports& NewExports = LibraryExportMap.Add(InLibrary);
		FUtils::GetExportedParametersForLibrary(InLibrary, NewExports);
		return NewExports;
	};
	
	// Find all blocks and their bound parameters
	if(Args.bShowBoundParameters)
	{
		ARFilter.ClassPaths = { UAnimNextParameterBlock::StaticClass()->GetClassPathName() };
		
		TArray<FAssetData> BlockAssets;
		AssetRegistry.GetAssets(ARFilter, BlockAssets);

		for(const FAssetData& BlockAsset : BlockAssets)
		{
			FAnimNextParameterBlockAssetRegistryExports Exports;
			if(FUtils::GetExportedBindingsForBlock(BlockAsset, Exports))
			{
				for(const FAnimNextParameterBlockAssetRegistryExportEntry& Export : Exports.Bindings)
				{
					const FSoftObjectPath LibraryAssetPath(Export.Library);
					FAssetData LibraryAsset = AssetRegistry.GetAssetByObjectPath(LibraryAssetPath);
					if(LibraryAsset.IsValid())
					{
						BoundParameters.Add({ Export.Name, LibraryAsset });

						FParameterBindingReference NewReference(Export.Name, LibraryAsset, BlockAsset);
						if(!Args.OnFilterParameter.IsBound() || Args.OnFilterParameter.Execute(NewReference) == EFilterParameterResult::Include)
						{
							const FAnimNextParameterLibraryAssetRegistryExports& LibraryExports = GetExportsForLibrary(LibraryAsset);
							FAnimNextParamType ParamType = FUtils::GetParameterTypeFromLibraryExports(Export.Name, LibraryExports);
							if(!Args.OnFilterParameterType.IsBound() || Args.OnFilterParameterType.Execute(ParamType) == EFilterParameterResult::Include)
							{
								TSharedRef<FParameterPickerEntry> NewEntry = MakeShared<FParameterPickerEntry>(NewReference, ParamType);
								Entries.Add(NewEntry);
							}
						}
					}
				}
			};
		}
	}

	// Find all library parameters (that have not already been added as bound above)
	if(Args.bShowUnboundParameters)
	{
		ARFilter.ClassPaths = { UAnimNextParameterLibrary::StaticClass()->GetClassPathName() };

		TArray<FAssetData> LibraryAssets;
		AssetRegistry.GetAssets(ARFilter, LibraryAssets);

		for(const FAssetData& LibraryAsset : LibraryAssets)
		{
			const FAnimNextParameterLibraryAssetRegistryExports& Exports = GetExportsForLibrary(LibraryAsset);
			for(const FAnimNextParameterLibraryAssetRegistryExportEntry& Export : Exports.Parameters)
			{
				if(!BoundParameters.Contains( { Export.Name, LibraryAsset } ))
				{
					FParameterBindingReference NewReference(Export.Name, LibraryAsset);
					if(!Args.OnFilterParameter.IsBound() || Args.OnFilterParameter.Execute(NewReference) == EFilterParameterResult::Include)
					{
						if (!Args.OnFilterParameterType.IsBound() || Args.OnFilterParameterType.Execute(Export.Type) == EFilterParameterResult::Include)
						{
							TSharedRef<FParameterPickerEntry> NewEntry = MakeShared<FParameterPickerEntry>(NewReference, Export.Type);
							Entries.Add(NewEntry);
						}
					}
				}
			}
		}
	}

	BuildHierarchy();

	RefreshFilter();
}

void SParameterPicker::BuildHierarchy()
{
	Hierarchy.Reset();

	TMap<FName, TSharedRef<FParameterPickerEntry>> NameMap;
	NameMap.Reserve(Entries.Num());

	for (const TSharedRef<FParameterPickerEntry>& Entry : Entries)
	{
		NameMap.Add(Entry->Binding.Parameter, Entry);
	}

	TArray< TSharedRef<FParameterPickerEntry>> NewEntries;
	for (const TSharedRef<FParameterPickerEntry>& Entry : Entries)
	{
		// Parse name into separators
		TStringBuilder<256> WholeParameterString;
		Entry->Binding.Parameter.ToString(WholeParameterString);

		TStringBuilder<256> PartialParameterString;

		// We use '_' as a seperator here as:
		// - Each param is a UObject in editor and uses its object name, so we cannot use '.'
		// - This maps nicely to Verse tags that use '_' to hierarchically define their relationship
		TSharedPtr<FParameterPickerEntry> Parent = nullptr;
		UE::String::ParseTokens(WholeParameterString, TEXT('_'), [this, &PartialParameterString, &NameMap, &Parent, &NewEntries](FStringView InStringView)
		{
			if(Parent.IsValid())
			{
				// Have a parent, so add child if it doesnt exist already
				PartialParameterString += TEXT('_');
				PartialParameterString += InStringView;

				const FName ParameterName(InStringView);
				const FName PartialParameterName(PartialParameterString);
				if (TSharedRef<FParameterPickerEntry>* ExistingEntry = NameMap.Find(PartialParameterName))
				{
					(*ExistingEntry)->Parent = Parent;
					if (!Parent->Children.ContainsByPredicate([&ExistingEntry](const TSharedRef<FParameterPickerEntry>& InEntry) { return (*ExistingEntry)->Binding.Parameter == InEntry->Binding.Parameter; }))
					{
						Parent->Children.Add(*ExistingEntry);
					}
					Parent = *ExistingEntry;
				}
				else
				{
					TSharedRef<FParameterPickerEntry> NewEntry = MakeShared<FParameterPickerEntry>(PartialParameterName);
					NewEntry->Parent = Parent;
					if (!Parent->Children.ContainsByPredicate([&NewEntry](const TSharedRef<FParameterPickerEntry>& InEntry) { return NewEntry->Binding.Parameter == InEntry->Binding.Parameter; }))
					{
						Parent->Children.Add(NewEntry);
					}
					Parent = NameMap.Add(PartialParameterName, NewEntry);
					NewEntries.Add(NewEntry);
				}
			}
			else
			{
				PartialParameterString += InStringView;

				// Add root item if it doesnt exist already
				const FName ParameterName(InStringView);
				if (TSharedRef<FParameterPickerEntry>* ExistingParent = NameMap.Find(ParameterName))
				{
					Parent = *ExistingParent;
				}
				else
				{
					TSharedRef<FParameterPickerEntry> NewEntry = MakeShared<FParameterPickerEntry>(ParameterName);
					Parent = NameMap.Add(ParameterName, NewEntry);
					NewEntries.Add(NewEntry);
				}

				if (!Hierarchy.ContainsByPredicate([&Parent](const TSharedRef<FParameterPickerEntry>& InEntry){ return Parent->Binding.Parameter == InEntry->Binding.Parameter; }))
				{
					Hierarchy.Add(Parent.ToSharedRef());
				}
			}
		}, UE::String::EParseTokensOptions::SkipEmpty);
	}

	Entries.Append(NewEntries);
}

void SParameterPicker::RefreshFilter()
{
	FilteredEntries.Reset();
	FilteredHierarchy.Reset();
	const FString FilterTextAsString = FilterText.ToString();

	for (const TSharedRef<FParameterPickerEntry>& Entry : Entries)
	{
		Entry->FilterResult = FParameterPickerEntry::EFilterResult::DoesNotPassFilter;
	}

	for(const TSharedRef<FParameterPickerEntry>& Entry : Entries)
	{
		Entry->FilteredChildren.Reset();
		if(Entry->PassesFilter(FilterTextAsString))
		{
			Entry->FilterResult |= FParameterPickerEntry::EFilterResult::PassesFilter;
			FilteredEntries.Add(Entry);

			if(FilterTextAsString.Len() > 0)
			{
				TSharedPtr< FParameterPickerEntry> ParentEntry = Entry->Parent;
				while (ParentEntry.IsValid())
				{
					ParentEntry->FilterResult |= FParameterPickerEntry::EFilterResult::ChildPassesFilter;
					ParentEntry = ParentEntry->Parent;
				}
			}
		}
	}

	TArray<TSharedRef<FParameterPickerEntry>> Stack;
	Stack.Reserve(16);
	for (const TSharedRef<FParameterPickerEntry>& Entry : Hierarchy)
	{
		Stack.Add(Entry);
	}

	while (Stack.Num() > 0)
	{
		TSharedRef<FParameterPickerEntry> Top = Stack.Top();
		Stack.Pop(false);

		if (Top->FilterResult != FParameterPickerEntry::EFilterResult::DoesNotPassFilter)
		{
			if (!Top->Parent.IsValid())
			{
				FilteredHierarchy.Add(Top);
			}
			else
			{
				Top->Parent->FilteredChildren.Add(Top);
			}

			for (const TSharedRef<FParameterPickerEntry>& ChildEntry : Top->Children)
			{
				Stack.Add(ChildEntry);
			}

			if (EnumHasAnyFlags(Top->FilterResult, FParameterPickerEntry::EFilterResult::ChildPassesFilter))
			{
				EntriesList->SetItemExpansion(Top, true);
			}
		}
	}

	EntriesList->RequestTreeRefresh();
}

class SParameterPickerRow : public SMultiColumnTableRow<TSharedRef<FParameterPickerEntry>>
{
	SLATE_BEGIN_ARGS(SParameterPickerRow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, TSharedRef<FParameterPickerEntry> InEntry, TSharedRef<const SParameterPicker> InParameterPicker)
	{
		Entry = InEntry;
		ParameterPicker = InParameterPicker;
		SMultiColumnTableRow<TSharedRef<FParameterPickerEntry>>::Construct( SMultiColumnTableRow<TSharedRef<FParameterPickerEntry>>::FArguments(), InOwnerTableView);
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& InColumnName) override
	{
		using namespace ParameterPicker;

		if (InColumnName == Column_Type)
		{
			return
				SNew(SHorizontalBox)
				.Visibility(Entry->ParamType.IsValid() ? EVisibility::Visible : EVisibility::Hidden)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Padding(2.0f)
				.AutoWidth()
				[
					SNew(SImage)
					.Image(Entry->PinIcon)
					.ColorAndOpacity(Entry->PinColor)
				];
		}
		else if(InColumnName == Column_Parameter)
		{
			return
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Padding(2.0f)
				.AutoWidth()
				[
					SNew(SExpanderArrow, SharedThis(this))
					.IndentAmount(8)
				]
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding(2.0f)
				.FillWidth(1.0f)
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(FText::FromString(Entry->DisplayString))
					.ToolTipText(UncookedOnly::FUtils::GetParameterDisplayNameText(Entry->Binding.Parameter))
					.HighlightText_Lambda([this]()
					{
						return ParameterPicker.Pin()->FilterText;
					})
				];
		}
		else if(InColumnName == Column_Library)
		{
			if(Entry->Binding.Library.IsValid())
			{
				
				return
					SNew(SBox)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Font(IDetailLayoutBuilder::GetDetailFont())
						.Text(FText::FromName(Entry->Binding.Library.AssetName))
						.ToolTipText(FText::FromName(Entry->Binding.Library.PackageName))
					];
			}
		}
		else if(InColumnName == Column_Block)
		{
			if(Entry->Binding.Block.IsValid())
			{
				return
					SNew(SBox)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Font(IDetailLayoutBuilder::GetDetailFont())
						.Text(FText::FromName(Entry->Binding.Block.AssetName))
						.ToolTipText(FText::FromName(Entry->Binding.Block.PackageName))
					];
			}
		}

		return SNullWidget::NullWidget;
	}
	
	TWeakPtr<const SParameterPicker> ParameterPicker;
	TSharedPtr<FParameterPickerEntry> Entry;
};

TSharedRef<ITableRow> SParameterPicker::HandleGenerateRow(TSharedRef<FParameterPickerEntry> InEntry, const TSharedRef<STableViewBase>& InOwnerTable) const
{
	return SNew(SParameterPickerRow, InOwnerTable, InEntry, SharedThis(this));
}

void SParameterPicker::HandleGetChildren(TSharedRef<FParameterPickerEntry> InEntry, TArray<TSharedRef<FParameterPickerEntry>>& OutChildren) const
{
	OutChildren = InEntry->FilteredChildren;
}

void SParameterPicker::HandleSelectionChanged(TSharedPtr<FParameterPickerEntry> InEntry, ESelectInfo::Type InSelectInfo)
{
	Args.OnSelectionChanged.ExecuteIfBound();

	if(!Args.bMultiSelect && EntriesList->GetNumItemsSelected() == 1 && Args.OnParameterPicked.IsBound())
	{
		TArray<TSharedRef<FParameterPickerEntry>> SelectedEntries;
		EntriesList->GetSelectedItems(SelectedEntries);

		if(SelectedEntries[0]->ParamType.IsValid())
		{
			Args.OnParameterPicked.ExecuteIfBound(SelectedEntries[0]->Binding);
		}
	}
}

void SParameterPicker::HandleGetParameterBindings(TArray<FParameterBindingReference>& OutParameterBindings) const
{
	TArray<TSharedRef<FParameterPickerEntry>> SelectedEntries;
	EntriesList->GetSelectedItems(SelectedEntries);

	for(TSharedRef<FParameterPickerEntry>& Entry : SelectedEntries)
	{
		OutParameterBindings.Emplace(Entry->Binding);
	}
}

bool SParameterPicker::HandleIsSelectableOrNavigable(TSharedRef<FParameterPickerEntry> InEntry) const
{
	// Only allow selecting items that have valid parameters
	return InEntry->ParamType.IsValid();
}

}

#undef LOCTEXT_NAMESPACE