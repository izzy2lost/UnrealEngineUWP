// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SMVVMViewBindingListView.h"

#include "Bindings/MVVMBindingHelper.h"
#include "Blueprint/WidgetTree.h"
#include "BlueprintEditor.h"
#include "Details/WidgetPropertyDragDropOp.h"
#include "Dialog/SCustomDialog.h"
#include "DragAndDrop/DecoratedDragDropOp.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Views/TableViewMetadata.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/MessageDialog.h"

#include "MVVMBlueprintView.h"
#include "MVVMBlueprintViewBinding.h"
#include "MVVMBlueprintViewConversionFunction.h"
#include "MVVMBlueprintViewEvent.h"
#include "MVVMEditorSubsystem.h"
#include "MVVMDeveloperProjectSettings.h"
#include "MVVMWidgetBlueprintExtension_View.h"

#include "ScopedTransaction.h"
#include "SEnumCombo.h"
#include "SSimpleButton.h" 
#include "Styling/MVVMEditorStyle.h"
#include "Styling/StyleColors.h"
#include "ViewModelFieldDragDropOp.h"

#include "Widgets/SMVVMEventParameter.h"
#include "Widgets/SMVVMFieldEntry.h"
#include "Widgets/SMVVMFieldSelector.h"
#include "Widgets/SMVVMFunctionParameter.h"
#include "Widgets/SMVVMSourceSelector.h"
#include "Widgets/SMVVMViewBindingPanel.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SMVVMViewModelPanel.h" // IWYU pragma: keep
#include "Widgets/SMVVMViewModelBindingListWidget.h"
#include "Widgets/Views/STreeView.h"

#define LOCTEXT_NAMESPACE "BindingListView"

namespace UE::MVVM
{

/**
 * A wrapper around either a widget row or a binding row
 */
struct FBindingEntry
{
	enum class ERowType
	{
		None,
		Group,
		Binding,
		BindingParameter,
		Event,
		EventParameter,
	};

	FMVVMBlueprintViewBinding* GetBinding(UMVVMBlueprintView* View) const
	{
		return View->GetBinding(BindingId);
	}

	const FMVVMBlueprintViewBinding* GetBinding(const UMVVMBlueprintView* View) const
	{
		return View->GetBinding(BindingId);
	}

	ERowType GetRowType() const
	{
		return RowType;
	}

	FName GetGroupName() const
	{
		return Name;
	}

	FGuid GetGroupAsViewModel() const
	{
		return BindingId;
	}

	bool IsGroupWidget() const
	{
		return bGroupIsWidget;
	}

	void SetGroup(FName WidgetName)
	{
		check(RowType == ERowType::None);
		RowType = ERowType::Group;
		Name = WidgetName;
		bGroupIsWidget = true;
	}

	void SetGroup(FName ViewModelName, FGuid ViewModelId)
	{
		if (!ViewModelId.IsValid())
		{
			SetGroup(ViewModelName);
		}
		else
		{
			check(RowType == ERowType::None);
			RowType = ERowType::Group;
			Name = ViewModelName;
			BindingId = ViewModelId;
			bGroupIsWidget = false;
		}
	}

	FGuid GetBindingId() const
	{
		return BindingId;
	}

	void SetBindingId(FGuid Id)
	{
		check(RowType == ERowType::None);
		RowType = ERowType::Binding;
		BindingId = Id;
	}

	FName GetBindingParameterName() const
	{
		return Name;
	}

	void SetBindingParameterName(FGuid Id, FName ParameterName)
	{
		check(RowType == ERowType::None);
		RowType = ERowType::BindingParameter;
		BindingId = Id;
		Name = ParameterName;
	}

	UMVVMBlueprintViewEvent* GetEvent() const
	{
		return Event.Get();
	}

	void SetEvent(UMVVMBlueprintViewEvent* InEvent)
	{
		check(RowType == ERowType::None);
		RowType = ERowType::Event;
		Event = InEvent;
	}

	FName GetEventParameterName() const
	{
		return Name;
	}

	void SetEventParameterName(UMVVMBlueprintViewEvent* InEvent, FName ParameterName)
	{
		check(RowType == ERowType::None);
		RowType = ERowType::EventParameter;
		Event = InEvent;
		Name = ParameterName;
	}

	TConstArrayView<TSharedPtr<FBindingEntry>> GetAllChildren() const
	{
		return AllChildren;
	}

	TConstArrayView<TSharedPtr<FBindingEntry>> GetFilteredChildren() const
	{
		return bUseFilteredChildren ? FilteredChildren : AllChildren;
	}

	void AddChild(TSharedPtr<FBindingEntry> Child)
	{
		AllChildren.Add(Child);
	}

	void AddFilteredChild(TSharedPtr<FBindingEntry> Child)
	{
		FilteredChildren.Add(Child);
		bUseFilteredChildren = true;
	}

	void ResetChildren()
	{
		AllChildren.Reset();
		FilteredChildren.Reset();
		bUseFilteredChildren = false;
	}

	void SetUseFilteredChildList()
	{
		bUseFilteredChildren = true;
	}

	bool operator==(const FBindingEntry& Other) const
	{
		return RowType == Other.RowType &&
			Name == Other.Name &&
			BindingId == Other.BindingId;
	}

	FString GetSearchNameString(UMVVMBlueprintView* View, UWidgetBlueprint* WidgetBP)
	{
		FString RowToString;
		FString FunctionKeywords;

		switch (RowType)
		{
		case UE::MVVM::FBindingEntry::ERowType::Group:
		case UE::MVVM::FBindingEntry::ERowType::BindingParameter:
		case UE::MVVM::FBindingEntry::ERowType::EventParameter:
			RowToString = Name.ToString();
			break;
		case UE::MVVM::FBindingEntry::ERowType::Binding:
			{
				FMVVMBlueprintViewBinding* BindingInRow = GetBinding(View);
				check(BindingInRow);
				RowToString.Append(BindingInRow->GetSearchableString(WidgetBP));
			}
			break;
		case UE::MVVM::FBindingEntry::ERowType::Event:
			if (UMVVMBlueprintViewEvent* EventInRow = GetEvent())
			{
				RowToString.Append(EventInRow->GetSearchableString());
			}
			break;
		default:
			break;
		}

		RowToString.ReplaceInline(TEXT(" "), TEXT(""));

		return RowToString;
	}
private:
	ERowType RowType = ERowType::None;
	FName Name;
	FGuid BindingId;
	TWeakObjectPtr<UMVVMBlueprintViewEvent> Event;
	TArray<TSharedPtr<FBindingEntry>> AllChildren;
	TArray<TSharedPtr<FBindingEntry>> FilteredChildren;
	bool bGroupIsWidget = false;
	bool bUseFilteredChildren = false;
};

namespace Private
{
	TArray<FName>* GetBindingModeNames()
	{
		static TArray<FName> BindingModeNames;

		if (BindingModeNames.IsEmpty())
		{
			UEnum* ModeEnum = StaticEnum<EMVVMBindingMode>();

			BindingModeNames.Reserve(ModeEnum->NumEnums());

			for (int32 BindingIndex = 0; BindingIndex < ModeEnum->NumEnums() - 1; ++BindingIndex)
			{
				const bool bIsHidden = ModeEnum->HasMetaData(TEXT("Hidden"), BindingIndex);
				if (!bIsHidden)
				{
					BindingModeNames.Add(ModeEnum->GetNameByIndex(BindingIndex));
				}
			}
		}

		return &BindingModeNames;
	}

	void ExpandAll(const TSharedPtr<STreeView<TSharedPtr<FBindingEntry>>>& TreeView, const TSharedPtr<FBindingEntry>& Entry)
	{
		TreeView->SetItemExpansion(Entry, true);

		for (const TSharedPtr<FBindingEntry>& Child : Entry->GetFilteredChildren())
		{
			ExpandAll(TreeView, Child);
		}
	}

	TSharedPtr<FBindingEntry> FindBinding(FGuid BindingId, TConstArrayView<TSharedPtr<FBindingEntry>> Entries)
	{
		for (const TSharedPtr<FBindingEntry>& Entry : Entries)
		{
			if (Entry->GetRowType() == FBindingEntry::ERowType::Binding && Entry->GetBindingId() == BindingId)
			{
				return Entry;
			}
			TSharedPtr<FBindingEntry> Result = FindBinding(BindingId, Entry->GetAllChildren());
			if (Result)
			{
				return Result;
			}
		}
		return TSharedPtr<FBindingEntry>();
	}
	
	TSharedPtr<FBindingEntry> FindEvent(UMVVMBlueprintViewEvent* Event, TConstArrayView<TSharedPtr<FBindingEntry>> Entries)
	{
		for (const TSharedPtr<FBindingEntry>& Entry : Entries)
		{
			if (Entry->GetRowType() == FBindingEntry::ERowType::Event && Entry->GetEvent() == Event)
			{
				return Entry;
			}
			TSharedPtr<FBindingEntry> Result = FindEvent(Event, Entry->GetAllChildren());
			if (Result)
			{
				return Result;
			}
		}
		return TSharedPtr<FBindingEntry>();
	}

	void FilterEntryList(FString FilterString, const TArray<TSharedPtr<FBindingEntry>>& RootGroups, TArray<TSharedPtr<FBindingEntry>>& FilteredRootGroups, UMVVMBlueprintView* BlueprintView, UMVVMWidgetBlueprintExtension_View* MVVMExtensionPtr)
	{
		if (!FilterString.TrimStartAndEnd().IsEmpty())
		{
			TArray<FString> SearchKeywords;
			FilterString.ParseIntoArray(SearchKeywords, TEXT(" "));

			struct FIsAllKeywordsInString
			{
				bool operator()(FString EntryString, TArray<FString>& SearchKeywords)
				{
					for (const FString& Keyword : SearchKeywords)
					{
						if (!EntryString.Contains(Keyword))
						{
							return false;
						}
					}
					return true;
				}
			} IsAllKeywordsInString;

			struct FAddFilteredEntry
			{
				FAddFilteredEntry(FIsAllKeywordsInString& InIsAllKeywordsInString, TArray<FString>& InSearchKeywords, UMVVMBlueprintView* InBlueprintView, UWidgetBlueprint* InWidgetBlueprint)
					: IsAllKeywordsInString(InIsAllKeywordsInString)
					, SearchKeywords(InSearchKeywords)
					, WidgetBlueprint(InWidgetBlueprint)
					, BlueprintView(InBlueprintView)
				{}
				void operator()(TSharedPtr<FBindingEntry> ParentEntry)
				{
					for (TSharedPtr<FBindingEntry> Entry : ParentEntry->GetAllChildren())
					{
						FString EntryString = Entry->GetSearchNameString(BlueprintView, WidgetBlueprint);
						if (IsAllKeywordsInString(EntryString, SearchKeywords))
						{
							// If the filter text is found in the group name, we keep the entire group.
							ParentEntry->AddFilteredChild(Entry);
						}
						else
						{
							(*this)(Entry);
						}
					}
					ParentEntry->SetUseFilteredChildList();
				}
				FIsAllKeywordsInString& IsAllKeywordsInString;
				TArray<FString>& SearchKeywords;
				UWidgetBlueprint* WidgetBlueprint = nullptr;
				UMVVMBlueprintView* BlueprintView = nullptr;
			};
			FAddFilteredEntry AddFilteredEntry = FAddFilteredEntry(IsAllKeywordsInString, SearchKeywords, BlueprintView, MVVMExtensionPtr->GetWidgetBlueprint());

			for (const TSharedPtr<FBindingEntry>& GroupEntry : RootGroups)
			{
				FString EntryString = GroupEntry->GetSearchNameString(BlueprintView, MVVMExtensionPtr->GetWidgetBlueprint());

				// If the filter text is found in the group name, we keep the entire group.
				if (IsAllKeywordsInString(EntryString, SearchKeywords))
				{
					FilteredRootGroups.Add(GroupEntry);
				}
				else
				{
					AddFilteredEntry(GroupEntry);
					if (GroupEntry->GetFilteredChildren().Num() > 0)
					{
						FilteredRootGroups.Add(GroupEntry);
					}
				}
			}
		}
		else
		{
			FilteredRootGroups = RootGroups;
		}
	}
} // namespace

/**
 * 
 */
class SGroupRow : public STableRow<TSharedPtr<FBindingEntry>>
{
public:
	SLATE_BEGIN_ARGS(SGroupRow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& OwnerTableView, const TSharedPtr<FBindingEntry>& InEntry, UWidgetBlueprint* InWidgetBlueprint)
	{
		Entry = InEntry;
		WidgetBlueprintWeak = InWidgetBlueprint;

		STableRow<TSharedPtr<FBindingEntry>>::Construct(
			STableRow<TSharedPtr<FBindingEntry>>::FArguments()
			.Padding(2.0f)
			.Style(FMVVMEditorStyle::Get(), "BindingView.WidgetRow")
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(2, 1)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Fill)
				.AutoWidth()
				[
					SNew(SBox)
					.MinDesiredWidth(150)
					[
						SNew(SBindingContextSelector, InWidgetBlueprint)
						.ShowClear(false)
						.AutoRefresh(true)
						.ViewModels(false)
						.SelectedBindingSource(this, &SGroupRow::GetSelectedWidget)
						.OnSelectionChanged(this, &SGroupRow::SetSelectedWidget)
					]
				]
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(SSimpleButton)
					.Icon(FAppStyle::Get().GetBrush("Icons.Plus"))
					.IsEnabled_Lambda([this]() { return !Entry->GetGroupName().IsNone(); })
					.OnClicked(this, &SGroupRow::AddBinding)
				] 
			],
			OwnerTableView
		);

		TSharedPtr<SWidget> ChildContent = ChildSlot.DetachWidget();
		ChildSlot
		[
			/* Add a single pixel top and bottom border for this widget. */
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
			.Padding(0, 1)
			[
				/* Restore the border that we're meant to have that reacts to selection/hover/etc. */
				SNew(SBorder)
				.BorderImage(this, &SGroupRow::GetBorderImage)
				.Padding(0)
				[
					ChildContent.ToSharedRef()
				]
			]
		];
	}

private:
	FBindingSource GetSelectedWidget() const
	{
		return Entry->IsGroupWidget()
			? FBindingSource::CreateForWidget(WidgetBlueprintWeak.Get(), Entry->GetGroupName())
			: FBindingSource::CreateForViewModel(WidgetBlueprintWeak.Get(), Entry->GetGroupAsViewModel());
	}

	void SetSelectedWidget(FBindingSource Source)
	{
		if (UWidgetBlueprint* WidgetBlueprint = WidgetBlueprintWeak.Get())
		{
			UMVVMEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
			UMVVMBlueprintView* View = EditorSubsystem->GetView(WidgetBlueprint);

			for (const TSharedPtr<FBindingEntry>& ChildEntry : Entry->GetAllChildren())
			{
				if (ChildEntry->GetRowType() == FBindingEntry::ERowType::Binding)
				{
					if (FMVVMBlueprintViewBinding* Binding = ChildEntry->GetBinding(View))
					{
						FMVVMBlueprintPropertyPath CurrentPath = Binding->DestinationPath;
						Source.SetSourceTo(CurrentPath);

						EditorSubsystem->SetDestinationPathForBinding(WidgetBlueprint, *Binding, CurrentPath);
					}
				}

				if (ChildEntry->GetRowType() == FBindingEntry::ERowType::Event)
				{
					if (UMVVMBlueprintViewEvent* Event = ChildEntry->GetEvent())
					{
						FMVVMBlueprintPropertyPath CurrentPath = Event->GetEventPath();
						Source.SetSourceTo(CurrentPath);

						EditorSubsystem->SetEventPath(Event, CurrentPath);
					}
				}
			}
		}
	}

	FReply AddBinding() const
	{
		if (UWidgetBlueprint* WidgetBlueprint = WidgetBlueprintWeak.Get())
		{
			UMVVMEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
			FMVVMBlueprintViewBinding& Binding = EditorSubsystem->AddBinding(WidgetBlueprint);
			FMVVMBlueprintPropertyPath Path;
			if (Entry->IsGroupWidget())
			{
				if (Entry->GetGroupName() == WidgetBlueprint->GetFName())
				{
					Path.SetSelfContext();
				}
				else
				{
					Path.SetWidgetName(Entry->GetGroupName());
				}
			}
			else
			{
				Path.SetViewModelId(Entry->GetGroupAsViewModel());
			}
			
			EditorSubsystem->SetDestinationPathForBinding(WidgetBlueprint, Binding, Path);
		}

		return FReply::Handled();
	}

private:
	TSharedPtr<FBindingEntry> Entry;
	TWeakObjectPtr<UWidgetBlueprint> WidgetBlueprintWeak;
};

/**
 * 
 */
class SBindingRow : public STableRow<TSharedPtr<FBindingEntry>>
{
public:
	SLATE_BEGIN_ARGS(SBindingRow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& OwnerTableView, const TSharedPtr<FBindingEntry>& InEntry, UWidgetBlueprint* InWidgetBlueprint)
	{
		Entry = InEntry;
		WidgetBlueprintWeak = InWidgetBlueprint;

		static IConsoleVariable* StaticCVarDefaultExecutionMode = IConsoleManager::Get().FindConsoleVariable(TEXT("MVVM.DefaultExecutionMode"));
		CVarDefaultExecutionMode = StaticCVarDefaultExecutionMode;
		
		FMVVMBlueprintViewBinding* ViewBinding = GetThisViewBinding();

		STableRow<TSharedPtr<FBindingEntry>>::Construct(
			STableRow<TSharedPtr<FBindingEntry>>::FArguments()
			.ShowWires(true)
			.Style(FMVVMEditorStyle::Get(), "BindingView.BindingRow")
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::Get().GetBrush("PlainBorder"))
			    .Padding(0)
			    .BorderBackgroundColor(this, &SBindingRow::GetErrorBorderColor)
			     [
					SNew(SBox)
					.HeightOverride(30)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.Padding(2, 0)
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Left) 
						.AutoWidth()
						[
							SNew(SCheckBox)
							.IsChecked(this, &SBindingRow::IsBindingCompiled)
							.OnCheckStateChanged(this, &SBindingRow::OnIsBindingCompileChanged)
						]

						+ SHorizontalBox::Slot()
						.Padding(2, 0)
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Left) 
						.AutoWidth()
						[
							SNew(SSimpleButton)
							.Icon(FAppStyle::Get().GetBrush("Icons.Error"))
							.Visibility(this, &SBindingRow::GetErrorButtonVisibility)
							.ToolTipText(this, &SBindingRow::GetErrorButtonToolTip)
							.OnClicked(this, &SBindingRow::OnErrorButtonClicked)
						]

						+ SHorizontalBox::Slot()
						.Padding(4, 0)
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Left)
						.AutoWidth()
						[
							SNew(SBox)
							.MinDesiredWidth(150)
							[
								SNew(SFieldSelector, InWidgetBlueprint)
								.OnGetPropertyPath(this, &SBindingRow::GetSelectedPropertyPath, false)
								.OnGetConversionFunction(this, &SBindingRow::GetSelectedConversionFunction, false)
								.OnFieldSelectionChanged(this, &SBindingRow::HandleFieldSelectionChanged, false)
								.OnGetSelectionContext(this, &SBindingRow::GetSelectedSelectionContext, false)
								.OnDrop(this, &SBindingRow::HandleFieldSelectorDrop, false)
								.OnDragEnter(this, &SBindingRow::HandleFieldSelectorDragEnter, false)
								.ShowContext(false)
							]
						]

						+ SHorizontalBox::Slot()
						.Padding(4, 0)
						.VAlign(VAlign_Fill)
						.HAlign(HAlign_Left) 
						.AutoWidth()
						[
							SNew(SComboBox<FName>)
							.OptionsSource(Private::GetBindingModeNames())
							.InitiallySelectedItem(StaticEnum<EMVVMBindingMode>()->GetNameByValue((int64) ViewBinding->BindingType))
							.OnSelectionChanged(this, &SBindingRow::OnBindingModeSelectionChanged)
							.OnGenerateWidget(this, &SBindingRow::GenerateBindingModeWidget)
							.ToolTipText(this, &SBindingRow::GetCurrentBindingModeLabel)
							.Content()
							[
								SNew(SBox)
								.HAlign(HAlign_Center)
								.VAlign(VAlign_Center)
								.WidthOverride(16)
								.HeightOverride(16)
								[
									SNew(SImage)
									.Image(this, &SBindingRow::GetCurrentBindingModeBrush)
								]
							]
						]
				
						+ SHorizontalBox::Slot()
						.Padding(4, 0, 2, 0)
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Left)
						.AutoWidth()
						[
							SNew(SBox)
							.MinDesiredWidth(150)
							[
								SNew(SFieldSelector, InWidgetBlueprint)
								.OnGetPropertyPath(this, &SBindingRow::GetSelectedPropertyPath, true)
								.OnGetConversionFunction(this, &SBindingRow::GetSelectedConversionFunction, true)
								.OnFieldSelectionChanged(this, &SBindingRow::HandleFieldSelectionChanged, true)
								.OnGetSelectionContext(this, &SBindingRow::GetSelectedSelectionContext, true)
								.OnDrop(this, &SBindingRow::HandleFieldSelectorDrop, true)
								.OnDragEnter(this, &SBindingRow::HandleFieldSelectorDragEnter, true)
							]
						]

						+ SHorizontalBox::Slot()
						[
							SNew(SSpacer)
						]

						+ SHorizontalBox::Slot()
						.Padding(2, 1)
						.VAlign(VAlign_Fill)
						.HAlign(HAlign_Right)
						.AutoWidth()
						[
							SNew(SCheckBox)
							.IsChecked(this, &SBindingRow::IsExecutionModeOverrideChecked)
							.OnCheckStateChanged(this, &SBindingRow::OnExecutionModeOverrideChanged)
						]

						+ SHorizontalBox::Slot()
						.Padding(2, 1)
						.VAlign(VAlign_Fill)
						.HAlign(HAlign_Right)
						.AutoWidth()
						[
							SNew(SComboButton)
							.ContentPadding(FMargin(4.f, 0.f))
							.OnGetMenuContent(this, &SBindingRow::OnGetExecutionModeMenuContent)
							.IsEnabled(this, &SBindingRow::IsExecutionModeOverridden)
							.ButtonContent()
							[
								SNew(STextBlock)
								.Text(this, &SBindingRow::GetExecutioModeValue)
								.ToolTipText(this, &SBindingRow::GetExecutioModeValueToolTip)
							]
						]
					]
				]
			],
			OwnerTableView
		);

		TSharedPtr<SWidget> ChildContent = ChildSlot.DetachWidget();
		ChildSlot
		[
			/* Add a single pixel top and bottom border for this widget. */
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
			.Padding(0, 2, 0, 1)
			[
				/* Restore the border that we're meant to have that reacts to selection/hover/etc. */
				SNew(SBorder)
				.BorderImage(this, &SBindingRow::GetBorderImage)
				.Padding(0)
				[
					ChildContent.ToSharedRef()
				]
			]
		];
	}

	FMVVMBlueprintViewBinding* GetThisViewBinding() const
	{
		UMVVMEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
		if (UMVVMBlueprintView* BlueprintViewPtr = EditorSubsystem->GetView(WidgetBlueprintWeak.Get()))
		{
			FMVVMBlueprintViewBinding* ViewBinding = Entry->GetBinding(BlueprintViewPtr);
			return ViewBinding;
		}
		return nullptr;
	}

private:

	FSlateColor GetErrorBorderColor() const
	{
		if (const FMVVMBlueprintViewBinding* ViewModelBinding = GetThisViewBinding())
		{
			UMVVMEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
			UMVVMBlueprintView* BlueprintViewPtr = EditorSubsystem->GetView(WidgetBlueprintWeak.Get());
			if (BlueprintViewPtr->HasBindingMessage(ViewModelBinding->BindingId, EBindingMessageType::Error))
			{
				return FStyleColors::Error;
			}
			else if (BlueprintViewPtr->HasBindingMessage(ViewModelBinding->BindingId, EBindingMessageType::Warning))
			{
				return FStyleColors::Warning;
			}
		}
		return FStyleColors::Transparent;
	}

	ECheckBoxState IsBindingEnabled() const
	{
		if (const FMVVMBlueprintViewBinding* ViewModelBinding = GetThisViewBinding())
		{
			return ViewModelBinding->bEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		}
		return ECheckBoxState::Undetermined;
	}

	ECheckBoxState IsBindingCompiled() const
	{
		if (const FMVVMBlueprintViewBinding* ViewModelBinding = GetThisViewBinding())
		{
			return ViewModelBinding->bCompile ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		}
		return ECheckBoxState::Undetermined;
	}

	EVisibility GetErrorButtonVisibility() const
	{
		if (const FMVVMBlueprintViewBinding* ViewModelBinding = GetThisViewBinding())
		{
			UMVVMEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
			UMVVMBlueprintView* BlueprintViewPtr = EditorSubsystem->GetView(WidgetBlueprintWeak.Get());
			bool HasBindingError = BlueprintViewPtr->HasBindingMessage(GetThisViewBinding()->BindingId, EBindingMessageType::Error) || BlueprintViewPtr->HasBindingMessage(GetThisViewBinding()->BindingId, EBindingMessageType::Warning);
			return HasBindingError ? EVisibility::Visible : EVisibility::Collapsed;
		}
		return EVisibility::Collapsed;
	}

	FText GetErrorButtonToolTip() const
	{
		// Get error messages of this binding
		if (const FMVVMBlueprintViewBinding* ViewModelBinding = GetThisViewBinding())
		{
			UMVVMEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
			UMVVMBlueprintView* BlueprintViewPtr = EditorSubsystem->GetView(WidgetBlueprintWeak.Get());
			TArray<FText> BindingErrorList = BlueprintViewPtr->GetBindingMessages(ViewModelBinding->BindingId, EBindingMessageType::Error);
			TArray<FText> BindingWarningList = BlueprintViewPtr->GetBindingMessages(ViewModelBinding->BindingId, EBindingMessageType::Warning);
			BindingErrorList.Append(BindingWarningList);

			static const FText NewLineText = FText::FromString(TEXT("\n"));
			FText HintText = LOCTEXT("ErrorButtonText", "Errors and Warnings: (Click to show in a separate window)");
			FText ErrorsText = FText::Join(NewLineText, BindingErrorList);
			return FText::Join(NewLineText, HintText, ErrorsText);
		}
		return FText();
	}

	FReply OnErrorButtonClicked()
	{
		ErrorDialog.Reset();
		ErrorItems.Reset();

		const UWidgetBlueprint* WidgetBlueprint = WidgetBlueprintWeak.Get();
		UMVVMEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
		if (const UMVVMBlueprintView* View = EditorSubsystem->GetView(WidgetBlueprint))
		{
			if (const FMVVMBlueprintViewBinding* ViewBinding = Entry->GetBinding(View))
			{
				for (const FText& ErrorText : View->GetBindingMessages(ViewBinding->BindingId, EBindingMessageType::Error))
				{
					ErrorItems.Add(MakeShared<FText>(ErrorText));
				}

				for (const FText& WarningText : View->GetBindingMessages(ViewBinding->BindingId, EBindingMessageType::Warning))
				{
					ErrorItems.Add(MakeShared<FText>(WarningText));
				}

				const FText BindingDisplayName = FText::FromString(ViewBinding->GetDisplayNameString(WidgetBlueprint));
				ErrorDialog = SNew(SCustomDialog)
					.Title(FText::Format(LOCTEXT("Compilation Errors and Warnings", "Compilation Errors and Warnings for {0}"), BindingDisplayName))
					.Buttons({
						SCustomDialog::FButton(LOCTEXT("OK", "OK"))
					})
					.Content()
					[
						SNew(SListView<TSharedPtr<FText>>)
						.ListItemsSource(&ErrorItems)
						.OnGenerateRow(this, &SBindingRow::OnGenerateErrorRow)
					];

				ErrorDialog->Show();
			}
		}

		return FReply::Handled();
	}

	TSharedRef<ITableRow> OnGenerateErrorRow(TSharedPtr<FText> Text, const TSharedRef<STableViewBase>& TableView) const
	{
		return SNew(STableRow<TSharedPtr<FText>>, TableView)
			.Content()
			[
				SNew(SEditableTextBox)
				.BackgroundColor(FStyleColors::Background)
				.IsReadOnly(true)
				.Text(*Text.Get())
			];
	}

	TArray<FBindingSource> GetAvailableViewModels() const
	{
		UMVVMEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
		return EditorSubsystem->GetAllViewModels(WidgetBlueprintWeak.Get());
	}

	FMVVMBlueprintPropertyPath GetSelectedPropertyPath(bool bSource) const
	{
		if (FMVVMBlueprintViewBinding* ViewBinding = GetThisViewBinding())
		{
			return bSource ? ViewBinding->SourcePath : ViewBinding->DestinationPath;
		}
		return FMVVMBlueprintPropertyPath();
	}

	const UFunction* GetSelectedConversionFunction(bool bSourceToDest) const
	{
		if (FMVVMBlueprintViewBinding* ViewBinding = GetThisViewBinding())
		{
			UMVVMEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
			return EditorSubsystem->GetConversionFunction(WidgetBlueprintWeak.Get(), *ViewBinding, bSourceToDest);
		}

		return nullptr;
	}

	void HandleFieldSelectionChanged(FMVVMBlueprintPropertyPath SelectedField, const UFunction* Function, bool bSource)
	{
		UWidgetBlueprint* WidgetBlueprintPtr = WidgetBlueprintWeak.Get();
		if (FMVVMBlueprintViewBinding* ViewBinding = GetThisViewBinding())
		{
			UMVVMEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
			if (bSource)
			{
				Subsystem->SetSourceToDestinationConversionFunction(WidgetBlueprintPtr, *ViewBinding, Function);
				if (ViewBinding->SourcePath != SelectedField)
				{
					Subsystem->SetSourcePathForBinding(WidgetBlueprintPtr, *ViewBinding, SelectedField);
				}
			}
			else
			{
				Subsystem->SetDestinationToSourceConversionFunction(WidgetBlueprintPtr, *ViewBinding, Function);
				if (ViewBinding->DestinationPath != SelectedField)
				{
					Subsystem->SetDestinationPathForBinding(WidgetBlueprintPtr, *ViewBinding, SelectedField);
				}
			}
		}
	}

	FFieldSelectionContext GetSelectedSelectionContext(bool bSource) const
	{
		FFieldSelectionContext Result;

		const UWidgetBlueprint* WidgetBlueprintPtr = WidgetBlueprintWeak.Get();
		if (WidgetBlueprintPtr == nullptr)
		{
			return Result;
		}
		
		if (const FMVVMBlueprintViewBinding* ViewBinding = GetThisViewBinding())
		{
			Result.BindingMode = ViewBinding->BindingType;

			{
				TArray<FMVVMConstFieldVariant> Fields = bSource ? ViewBinding->DestinationPath.GetFields(WidgetBlueprintPtr->SkeletonGeneratedClass) : ViewBinding->SourcePath.GetFields(WidgetBlueprintPtr->SkeletonGeneratedClass);
				if (Fields.Num() > 0)
				{
					FMVVMConstFieldVariant LastField = Fields.Last();
					if (LastField.IsProperty())
					{
						Result.AssignableTo = LastField.GetProperty();
					}
					else if (LastField.IsFunction())
					{
						if (const UFunction* Function = LastField.GetFunction())
						{
							Result.AssignableTo = BindingHelper::GetReturnProperty(Function);
						}
					}
				}
			}

			if (!bSource && !ViewBinding->DestinationPath.GetWidgetName().IsNone())
			{
				Result.FixedBindingSource = FBindingSource::CreateForWidget(WidgetBlueprintPtr, ViewBinding->DestinationPath.GetWidgetName());
			}

			Result.bAllowWidgets = true;
			Result.bAllowViewModels = bSource;
			Result.bAllowConversionFunctions = false;
			bool bIsReadingValue = (IsForwardBinding(ViewBinding->BindingType) && bSource)
				|| (IsBackwardBinding(ViewBinding->BindingType) && !bSource);
			if (!(IsBackwardBinding(ViewBinding->BindingType) && IsForwardBinding(ViewBinding->BindingType)))
			{
				Result.bAllowConversionFunctions = bIsReadingValue;
			}

			Result.bReadable = bIsReadingValue;
			Result.bWritable = (IsForwardBinding(ViewBinding->BindingType) && !bSource)
				|| (IsBackwardBinding(ViewBinding->BindingType) && bSource);
		}
		return Result;
	}

	FReply HandleFieldSelectorDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent, bool bSource)
	{
		TSharedPtr<FDecoratedDragDropOp> DragDropOp = DragDropEvent.GetOperationAs<FDecoratedDragDropOp>();
		if (!DragDropOp.IsValid())
		{
			return FReply::Unhandled();
		}

		// Accept all drag-drop operations that are widget properties, but only accept view model fields when we are dropping into the Source box.
		if (!DragDropOp->IsOfType<FWidgetPropertyDragDropOp>() && (!DragDropOp->IsOfType<FViewModelFieldDragDropOp>() || !bSource))
		{
			return FReply::Unhandled();
		}

		if (FMVVMBlueprintViewBinding* ViewBinding = GetThisViewBinding())
		{
			if (UWidgetBlueprint* WidgetBlueprintPtr = WidgetBlueprintWeak.Get())
			{
				TSharedPtr<FViewModelFieldDragDropOp> ViewModelFieldDragDropOp = DragDropEvent.GetOperationAs<FViewModelFieldDragDropOp>();
				TSharedPtr<FWidgetPropertyDragDropOp> WidgetPropertyDragDropOp = DragDropEvent.GetOperationAs<FWidgetPropertyDragDropOp>();
				bool bIsViewModelProperty = false;

				if (ViewModelFieldDragDropOp)
				{
					bIsViewModelProperty = true;
				}

				UWidgetBlueprint* DragDropWidgetBP = bIsViewModelProperty ? ViewModelFieldDragDropOp->WidgetBP.Get() : WidgetPropertyDragDropOp->WidgetBP.Get();

				if (WidgetBlueprintPtr == DragDropWidgetBP)
				{
					TArray<FFieldVariant> FieldPath = bIsViewModelProperty ? ViewModelFieldDragDropOp->DraggedField : WidgetPropertyDragDropOp->DraggedPropertyPath;
					FMVVMBlueprintPropertyPath PropertyPath;

					PropertyPath.ResetPropertyPath();
					for (const FFieldVariant& Field : FieldPath)
					{
						PropertyPath.AppendPropertyPath(WidgetBlueprintPtr, FMVVMConstFieldVariant(Field));
					}

					UMVVMEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
					if (bIsViewModelProperty)
					{
						if (ViewModelFieldDragDropOp->ViewModelId.IsValid())
						{
							PropertyPath.SetViewModelId(ViewModelFieldDragDropOp->ViewModelId);
						}
						else
						{
							return FReply::Unhandled();
						}
					}
					else
					{
						if (UWidget* OwnerWidgetPtr = WidgetPropertyDragDropOp->OwnerWidget.Get())
						{
							// Search for the widget by its name in the widget tree
							// If the widget is not found, we know it is the root preview widget so we use the blueprint name.
							if (WidgetBlueprintPtr->WidgetTree->FindWidget(OwnerWidgetPtr->GetFName()) && WidgetBlueprintPtr->GetFName() != OwnerWidgetPtr->GetFName())
							{
								PropertyPath.SetWidgetName(OwnerWidgetPtr->GetFName());
							}
							else
							{
								PropertyPath.SetSelfContext();
							}
						}
					}

					if (bSource)
					{
						Subsystem->SetSourceToDestinationConversionFunction(WidgetBlueprintPtr, *ViewBinding, nullptr);
						Subsystem->SetSourcePathForBinding(WidgetBlueprintPtr, *ViewBinding, PropertyPath);
					}
					else
					{
						Subsystem->SetDestinationToSourceConversionFunction(WidgetBlueprintPtr, *ViewBinding, nullptr);
						Subsystem->SetDestinationPathForBinding(WidgetBlueprintPtr, *ViewBinding, PropertyPath);
					}
					return FReply::Handled();
				}
			}
		}		
		return FReply::Unhandled();
	}

	void HandleFieldSelectorDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent, bool bSource)
	{
		TSharedPtr<FDecoratedDragDropOp> DragDropOp = DragDropEvent.GetOperationAs<FDecoratedDragDropOp>();
		if (DragDropOp.IsValid())
		{
			// Accept all drag-drop operations that are widget properties, but only accept view model fields when we are dropping into the Source box.
			if (DragDropOp->IsOfType<FWidgetPropertyDragDropOp>() || (DragDropOp->IsOfType<FViewModelFieldDragDropOp>() && bSource))
			{
				if (UWidgetBlueprint* WidgetBlueprintPtr = WidgetBlueprintWeak.Get())
				{
					TSharedPtr<FViewModelFieldDragDropOp> ViewModelFieldDragDropOp = DragDropEvent.GetOperationAs<FViewModelFieldDragDropOp>();
					TSharedPtr<FWidgetPropertyDragDropOp> WidgetPropertyDragDropOp = DragDropEvent.GetOperationAs<FWidgetPropertyDragDropOp>();
					bool IsViewModelProperty = false;

					if (ViewModelFieldDragDropOp)
					{
						IsViewModelProperty = true;
					}

					UWidgetBlueprint* DragDropWidgetBP = IsViewModelProperty ? ViewModelFieldDragDropOp->WidgetBP.Get() : WidgetPropertyDragDropOp->WidgetBP.Get();

					if (DragDropWidgetBP && DragDropWidgetBP == WidgetBlueprintPtr)
					{
						DragDropOp->CurrentIconBrush = FAppStyle::GetBrush(TEXT("Graph.ConnectorFeedback.OK"));
						return;
					}
				}
			}	
			else
			{
				DragDropOp->CurrentIconBrush = FAppStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error"));
			}
		}
	}

	ECheckBoxState IsExecutionModeOverrideChecked() const
	{
		if (FMVVMBlueprintViewBinding* ViewBinding = GetThisViewBinding())
		{
			return ViewBinding->bOverrideExecutionMode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		}
		return ECheckBoxState::Undetermined;
	}

	void OnExecutionModeOverrideChanged(ECheckBoxState NewState)
	{
		if (FMVVMBlueprintViewBinding* ViewBinding = GetThisViewBinding())
		{
			if (NewState == ECheckBoxState::Checked)
			{
				UMVVMEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
				EMVVMExecutionMode ExecutionMode = CVarDefaultExecutionMode ? (EMVVMExecutionMode)CVarDefaultExecutionMode->GetInt() : EMVVMExecutionMode::Immediate;
				Subsystem->OverrideExecutionModeForBinding(WidgetBlueprintWeak.Get(), *ViewBinding, ExecutionMode);
			}
			else
			{
				UMVVMEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
				Subsystem->ResetExecutionModeForBinding(WidgetBlueprintWeak.Get(), *ViewBinding);
			}
		}
	}

	void OnExecutionModeSelectionChanged(EMVVMExecutionMode Value)
	{
		if (FMVVMBlueprintViewBinding* ViewBinding = GetThisViewBinding())
		{
			UMVVMEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
			Subsystem->OverrideExecutionModeForBinding(WidgetBlueprintWeak.Get(), *ViewBinding, Value);
		}
	}

	TSharedRef<SWidget> OnGetExecutionModeMenuContent()
	{
		const bool bCloseAfterSelection = true;
		FMenuBuilder MenuBuilder(bCloseAfterSelection, nullptr, nullptr, true);

		UEnum* Enum = StaticEnum<EMVVMExecutionMode>();
		for (int32 Index = 0; Index < Enum->NumEnums() - 1; Index++)
		{
			EMVVMExecutionMode Mode = static_cast<EMVVMExecutionMode>(Enum->GetValueByIndex(Index));
			if (!GetDefault<UMVVMDeveloperProjectSettings>()->IsExecutionModeAllowed(Mode))
			{
				continue;
			}

			MenuBuilder.AddMenuEntry(
				Enum->GetDisplayNameTextByIndex(Index),
				Enum->GetToolTipTextByIndex(Index),
				FSlateIcon(),
				FUIAction
				(
					FExecuteAction::CreateLambda([this, Mode]()
					{
						OnExecutionModeSelectionChanged(Mode);
					})
				)
			);
		}

		return MenuBuilder.MakeWidget();
	}

	FText GetExecutioModeValue() const
	{
		EMVVMExecutionMode ExecutionMode = CVarDefaultExecutionMode ? (EMVVMExecutionMode)CVarDefaultExecutionMode->GetInt() : EMVVMExecutionMode::Immediate;
		if (FMVVMBlueprintViewBinding* ViewBinding = GetThisViewBinding())
		{
			if (ViewBinding->bOverrideExecutionMode)
			{
				ExecutionMode = ViewBinding->OverrideExecutionMode;
			}
		}
		return StaticEnum<EMVVMExecutionMode>()->GetDisplayNameTextByValue(static_cast<int64>(ExecutionMode));
	}
	
	FText GetExecutioModeValueToolTip() const
	{
		EMVVMExecutionMode ExecutionMode = CVarDefaultExecutionMode ? (EMVVMExecutionMode)CVarDefaultExecutionMode->GetInt() : EMVVMExecutionMode::Immediate;
		if (FMVVMBlueprintViewBinding* ViewBinding = GetThisViewBinding())
		{
			if (ViewBinding->bOverrideExecutionMode)
			{
				ExecutionMode = ViewBinding->OverrideExecutionMode;
			}
		}
		return StaticEnum<EMVVMExecutionMode>()->GetToolTipTextByIndex(static_cast<int64>(ExecutionMode));
	}
	
	bool IsExecutionModeOverridden() const
	{
		if (FMVVMBlueprintViewBinding* ViewBinding = GetThisViewBinding())
		{
			return ViewBinding->bOverrideExecutionMode;
		}
		return false;
	}

	void OnIsBindingEnableChanged(ECheckBoxState NewState)
	{
		if (NewState == ECheckBoxState::Undetermined)
		{
			return;
		}

		if (FMVVMBlueprintViewBinding* ViewBinding = GetThisViewBinding())
		{
			UMVVMEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
			Subsystem->SetEnabledForBinding(WidgetBlueprintWeak.Get(), *ViewBinding, NewState == ECheckBoxState::Checked);
		}
	}

	void OnIsBindingCompileChanged(ECheckBoxState NewState)
	{
		if (NewState == ECheckBoxState::Undetermined)
		{
			return;
		}

		if (FMVVMBlueprintViewBinding* ViewBinding = GetThisViewBinding())
		{
			UMVVMEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
			Subsystem->SetCompileForBinding(WidgetBlueprintWeak.Get(), *ViewBinding, NewState == ECheckBoxState::Checked);
		}
	}

	const FSlateBrush* GetBindingModeBrush(EMVVMBindingMode BindingMode) const
	{
		switch (BindingMode)
		{
		case EMVVMBindingMode::OneTimeToDestination:
			return FMVVMEditorStyle::Get().GetBrush("BindingMode.OneTimeOneWay");
		case EMVVMBindingMode::OneWayToDestination:
			return FMVVMEditorStyle::Get().GetBrush("BindingMode.OneWayToSource");
		case EMVVMBindingMode::OneWayToSource:
			return FMVVMEditorStyle::Get().GetBrush("BindingMode.OneWay");
		case EMVVMBindingMode::OneTimeToSource:
			return nullptr;
		case EMVVMBindingMode::TwoWay:
			return FMVVMEditorStyle::Get().GetBrush("BindingMode.TwoWay");
		default:
			return nullptr;
		}
	}

	const FSlateBrush* GetCurrentBindingModeBrush() const
	{
		if (FMVVMBlueprintViewBinding* ViewBinding = GetThisViewBinding())
		{
			return GetBindingModeBrush(ViewBinding->BindingType);
		}
		return nullptr;
	}

	FText GetCurrentBindingModeLabel() const
	{
		if (FMVVMBlueprintViewBinding* ViewBinding = GetThisViewBinding())
		{
			return GetBindingModeLabel(ViewBinding->BindingType);
		}
		return FText::GetEmpty();
	}

	FText GetBindingModeLabel(EMVVMBindingMode BindingMode) const
	{
		static FText OneTimeToDestinationLabel = LOCTEXT("OneTimeToDestinationLabel", "One Time To Widget");
		static FText OneWayToDestinationLabel = LOCTEXT("OneWayToDestinationLabel", "One Way To Widget");
		static FText OneWayToSourceLabel = LOCTEXT("OneWayToSourceLabel", "One Way To View Model");
		static FText OneTimeToSourceLabel = LOCTEXT("OneTimeToSourceLabel", "One Time To View Model");
		static FText TwoWayLabel = LOCTEXT("TwoWayLabel", "Two Way");

		switch (BindingMode)
		{
		case EMVVMBindingMode::OneTimeToDestination:
			return OneTimeToDestinationLabel;
		case EMVVMBindingMode::OneWayToDestination:
			return OneWayToDestinationLabel;
		case EMVVMBindingMode::OneWayToSource:
			return OneWayToSourceLabel;
		case EMVVMBindingMode::OneTimeToSource:
			return OneTimeToSourceLabel;
		case EMVVMBindingMode::TwoWay:
			return TwoWayLabel;
		default:
			return FText::GetEmpty();
		}
	}

	TSharedRef<SWidget> GenerateBindingModeWidget(FName ValueName) const
	{
		const UEnum* ModeEnum = StaticEnum<EMVVMBindingMode>();
		int32 Index = ModeEnum->GetIndexByName(ValueName);
		EMVVMBindingMode MVVMBindingMode = EMVVMBindingMode(Index);
		return SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.WidthOverride(16)
				.HeightOverride(16)
				[
					SNew(SImage)
					.Image(GetBindingModeBrush(MVVMBindingMode))
				]
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0, 0, 0, 0)
			[
				SNew(STextBlock)
				.Text(GetBindingModeLabel(MVVMBindingMode))
				.ToolTipText(ModeEnum->GetToolTipTextByIndex(Index))
			];
	}

	void OnBindingModeSelectionChanged(FName ValueName, ESelectInfo::Type)
	{
		if (FMVVMBlueprintViewBinding* ViewBinding = GetThisViewBinding())
		{
			const UEnum* ModeEnum = StaticEnum<EMVVMBindingMode>();
			EMVVMBindingMode NewMode = (EMVVMBindingMode) ModeEnum->GetValueByName(ValueName);

			UMVVMEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
			Subsystem->SetBindingTypeForBinding(WidgetBlueprintWeak.Get(), *ViewBinding, NewMode);
		}
	}

private:
	TSharedPtr<FBindingEntry> Entry;
	TWeakObjectPtr<UWidgetBlueprint> WidgetBlueprintWeak;
	TSharedPtr<SCustomDialog> ErrorDialog;
	TArray<TSharedPtr<FText>> ErrorItems;
	IConsoleVariable* CVarDefaultExecutionMode = nullptr;
};

/**
*
*/
class SEventRow : public STableRow<TSharedPtr<FBindingEntry>>
{
public:
	SLATE_BEGIN_ARGS(SEventRow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& OwnerTableView, const TSharedPtr<FBindingEntry>& InEntry, UWidgetBlueprint* InWidgetBlueprint)
	{
		Entry = InEntry;
		ViewEvent = Entry->GetEvent();
		WidgetBlueprintWeak = InWidgetBlueprint;

		STableRow<TSharedPtr<FBindingEntry>>::Construct(
			STableRow<TSharedPtr<FBindingEntry>>::FArguments()
			.ShowWires(true)
			.Style(FMVVMEditorStyle::Get(), "BindingView.BindingRow")
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::Get().GetBrush("PlainBorder"))
				.Padding(0.0f)
				.BorderBackgroundColor(this, &SEventRow::GetErrorBorderColor)
				[
					SNew(SBox)
					.HeightOverride(30)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.Padding(2.0f, 0.0f)
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Left)
						.AutoWidth()
						[
							SNew(SCheckBox)
							.IsChecked(this, &SEventRow::IsEventCompiled)
							.OnCheckStateChanged(this, &SEventRow::OnIsEventCompileChanged)
						]

						+ SHorizontalBox::Slot()
						.Padding(2.0f, 0.0f)
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Left)
						.AutoWidth()
						[
							SNew(SSimpleButton)
							.Icon(FAppStyle::Get().GetBrush("Icons.Error"))
							.Visibility(this, &SEventRow::GetErrorButtonVisibility)
							.ToolTipText(this, &SEventRow::GetErrorButtonToolTip)
							.OnClicked(this, &SEventRow::OnErrorButtonClicked)
						]

						+ SHorizontalBox::Slot()
						.Padding(4.0f, 0.0f)
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Left)
						.AutoWidth()
						[
							SNew(SBox)
							.MinDesiredWidth(150.0f)
							[
								SNew(SFieldSelector, InWidgetBlueprint)
								.OnGetPropertyPath(this, &SEventRow::GetSelectedPropertyPath, true)
								.OnGetConversionFunction(this, &SEventRow::GetSelectedConversionFunction, true)
								.OnFieldSelectionChanged(this, &SEventRow::HandleFieldSelectionChanged, true)
								.OnGetSelectionContext(this, &SEventRow::GetSelectedSelectionContext, true)
								.OnDrop(this, &SEventRow::HandleFieldSelectorDrop, true)
								.OnDragEnter(this, &SEventRow::HandleFieldSelectorDragEnter, true)
								.ShowContext(false)
							]
						]

						+ SHorizontalBox::Slot()
						.Padding(4.0f, 0.0f)
						.VAlign(VAlign_Fill)
						.HAlign(HAlign_Left)
						.AutoWidth()
						[
							SNew(SBox)
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Center)
							.WidthOverride(16.0f)
							.HeightOverride(16.0f)
							[
								SNew(SImage)
								.Image(FMVVMEditorStyle::Get().GetBrush("BindingMode.OneWay"))
							]
						]

						+ SHorizontalBox::Slot()
						.Padding(4.0f, 0.0f, 2.0f, 0.0f)
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Left)
						.AutoWidth()
						[
							SNew(SBox)
							.MinDesiredWidth(150.0f)
							[
								SNew(SFieldSelector, InWidgetBlueprint)
								.OnGetPropertyPath(this, &SEventRow::GetSelectedPropertyPath, false)
								.OnGetConversionFunction(this, &SEventRow::GetSelectedConversionFunction, false)
								.OnFieldSelectionChanged(this, &SEventRow::HandleFieldSelectionChanged, false)
								.OnGetSelectionContext(this, &SEventRow::GetSelectedSelectionContext, false)
								.OnDrop(this, &SEventRow::HandleFieldSelectorDrop, false)
								.OnDragEnter(this, &SEventRow::HandleFieldSelectorDragEnter, false)
							]
						]
					]
				]
			],
			OwnerTableView);

		TSharedPtr<SWidget> ChildContent = ChildSlot.DetachWidget();
		ChildSlot
		[
			/* Add a single pixel top and bottom border for this widget. */
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("Brushes.Recessed"))
			.Padding(0.0f, 2.0f, 0.0f, 1.0f)
			[
			/* Restore the border that we're meant to have that reacts to selection/hover/etc. */
				SNew(SBorder)
				.BorderImage(this, &SEventRow::GetBorderImage)
				.Padding(0.0f)
				[
					ChildContent.ToSharedRef()
				]
			]
		];
	}

private:
	FSlateColor GetErrorBorderColor() const
	{
		if (const UMVVMBlueprintViewEvent* Event = ViewEvent.Get())
		{
			if (Event->HasCompilationMessage(UMVVMBlueprintViewEvent::EMessageType::Error))
			{
				return FStyleColors::Error;
			}
			else if (Event->HasCompilationMessage(UMVVMBlueprintViewEvent::EMessageType::Warning))
			{
				return FStyleColors::Warning;
			}
		}
		return FStyleColors::Transparent;
	}

	EVisibility GetErrorButtonVisibility() const
	{
		if (const UMVVMBlueprintViewEvent* Event = ViewEvent.Get())
		{
			bool bHasBindingError = Event->HasCompilationMessage(UMVVMBlueprintViewEvent::EMessageType::Error);
			bool bHasBindingWarning = Event->HasCompilationMessage(UMVVMBlueprintViewEvent::EMessageType::Warning);
			return bHasBindingError || bHasBindingWarning ? EVisibility::Visible : EVisibility::Collapsed;
		}
		return EVisibility::Collapsed;
	}

	FText GetErrorButtonToolTip() const
	{
		if (const UMVVMBlueprintViewEvent* Event = ViewEvent.Get())
		{
			TArray<FText> BindingErrorList = Event->GetCompilationMessages(UMVVMBlueprintViewEvent::EMessageType::Error);
			TArray<FText> BindingWarningList = Event->GetCompilationMessages(UMVVMBlueprintViewEvent::EMessageType::Warning);
			BindingErrorList.Append(BindingWarningList);

			static const FText NewLineText = FText::FromString(TEXT("\n"));
			FText HintText = LOCTEXT("ErrorButtonText", "Errors and Warnings: (Click to show in a separate window)");
			FText ErrorsText = FText::Join(NewLineText, BindingErrorList);
			return FText::Join(NewLineText, HintText, ErrorsText);
		}
		return FText();
	}

	FReply OnErrorButtonClicked()
	{
		ErrorDialog.Reset();
		ErrorItems.Reset();

		if (const UMVVMBlueprintViewEvent* Event = ViewEvent.Get())
		{
			for (const FText& ErrorText : Event->GetCompilationMessages(UMVVMBlueprintViewEvent::EMessageType::Error))
			{
				ErrorItems.Add(MakeShared<FText>(ErrorText));
			}

			for (const FText& WarningText : Event->GetCompilationMessages(UMVVMBlueprintViewEvent::EMessageType::Warning))
			{
				ErrorItems.Add(MakeShared<FText>(WarningText));
			}

			const FText BindingDisplayName = Event->GetDisplayName(true);
			ErrorDialog = SNew(SCustomDialog)
				.Title(FText::Format(LOCTEXT("Compilation Errors and Warnings", "Compilation Errors and Warnings for {0}"), BindingDisplayName))
				.Buttons({
					SCustomDialog::FButton(LOCTEXT("OK", "OK"))
					})
				.Content()
				[
					SNew(SListView<TSharedPtr<FText>>)
					.ListItemsSource(&ErrorItems)
					.OnGenerateRow(this, &SEventRow::OnGenerateErrorRow)
				];

			ErrorDialog->Show();
		}

		return FReply::Handled();
	}

	TSharedRef<ITableRow> OnGenerateErrorRow(TSharedPtr<FText> Text, const TSharedRef<STableViewBase>& TableView) const
	{
		return SNew(STableRow<TSharedPtr<FText>>, TableView)
			.Content()
			[
				SNew(SEditableTextBox)
				.BackgroundColor(FStyleColors::Background)
				.IsReadOnly(true)
				.Text(*Text.Get())
			];
	}

	ECheckBoxState IsEventEnabled() const
	{
		if (const UMVVMBlueprintViewEvent* Event = ViewEvent.Get())
		{
			return Event->bEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		}
		return ECheckBoxState::Undetermined;
	}

	void OnIsEventEnableChanged(ECheckBoxState NewState)
	{
		if (NewState == ECheckBoxState::Undetermined)
		{
			return;
		}

		if (UMVVMBlueprintViewEvent* Event = ViewEvent.Get())
		{
			UMVVMEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
			Subsystem->SetEnabledForEvent(Event, NewState == ECheckBoxState::Checked);
		}
	}

	ECheckBoxState IsEventCompiled() const
	{
		if (const UMVVMBlueprintViewEvent* Event = ViewEvent.Get())
		{
			return Event->bCompile ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		}
		return ECheckBoxState::Undetermined;
	}

	void OnIsEventCompileChanged(ECheckBoxState NewState)
	{
		if (NewState == ECheckBoxState::Undetermined)
		{
			return;
		}

		if (UMVVMBlueprintViewEvent* Event = ViewEvent.Get())
		{
			UMVVMEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
			Subsystem->SetCompileForEvent(Event, NewState == ECheckBoxState::Checked);
		}
	}

	FMVVMBlueprintPropertyPath GetSelectedPropertyPath(bool bEvent) const
	{
		if (const UMVVMBlueprintViewEvent* Event = ViewEvent.Get())
		{
			return bEvent ? Event->GetEventPath() : Event->GetDestinationPath();
		}
		return FMVVMBlueprintPropertyPath();
	}

	const UFunction* GetSelectedConversionFunction(bool bSourceToDest) const
	{
		if (const UMVVMBlueprintViewEvent* Event = ViewEvent.Get())
		{
		}

		return nullptr;
	}

	void HandleFieldSelectionChanged(FMVVMBlueprintPropertyPath SelectedField, const UFunction* Function, bool bEvent)
	{
		if (UMVVMBlueprintViewEvent* Event = ViewEvent.Get())
		{
			UMVVMEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
			if (bEvent)
			{
				if (Event->GetEventPath() != SelectedField)
				{
					Subsystem->SetEventPath(Event, SelectedField);
				}
			}
			else
			{
				if (Event->GetDestinationPath() != SelectedField)
				{
					Subsystem->SetEventDestinationPath(Event, SelectedField);
				}
			}
		}
	}

	FFieldSelectionContext GetSelectedSelectionContext(bool bEvent) const
	{
		FFieldSelectionContext Result;
		const UWidgetBlueprint* WidgetBlueprintPtr = WidgetBlueprintWeak.Get();
		if (WidgetBlueprintPtr == nullptr)
		{
			return Result;
		}

		if (UMVVMBlueprintViewEvent* Event = ViewEvent.Get())
		{
			Result.BindingMode = EMVVMBindingMode::OneTimeToDestination;
			if (bEvent && !Event->GetEventPath().GetWidgetName().IsNone())
			{
				Result.FixedBindingSource = FBindingSource::CreateForWidget(WidgetBlueprintPtr, Event->GetEventPath().GetWidgetName());
			}

			Result.bAllowWidgets = true;
			Result.bAllowViewModels = !bEvent;
			Result.bAllowConversionFunctions = false;
			Result.bReadable = bEvent;
			Result.bWritable = !bEvent;
		}
		return Result;
	}

	FReply HandleFieldSelectorDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent, bool bEvent)
	{
		TSharedPtr<FDecoratedDragDropOp> DragDropOp = DragDropEvent.GetOperationAs<FDecoratedDragDropOp>();
		if (!DragDropOp.IsValid())
		{
			return FReply::Unhandled();
		}

		// Accept all drag-drop operations that are widget properties, but only accept view model fields when we are dropping into the Source box.
		if (!DragDropOp->IsOfType<FWidgetPropertyDragDropOp>() && (!DragDropOp->IsOfType<FViewModelFieldDragDropOp>() || !bEvent))
		{
			return FReply::Unhandled();
		}

		UMVVMBlueprintViewEvent* Event = ViewEvent.Get();
		UWidgetBlueprint* WidgetBlueprintPtr = WidgetBlueprintWeak.Get();
		if (Event == nullptr || WidgetBlueprintPtr == nullptr)
		{
			return FReply::Unhandled();
		}

		TSharedPtr<FViewModelFieldDragDropOp> ViewModelFieldDragDropOp = DragDropEvent.GetOperationAs<FViewModelFieldDragDropOp>();
		TSharedPtr<FWidgetPropertyDragDropOp> WidgetPropertyDragDropOp = DragDropEvent.GetOperationAs<FWidgetPropertyDragDropOp>();
		bool bIsViewModelProperty = ViewModelFieldDragDropOp != nullptr;

		UWidgetBlueprint* DragDropWidgetBP = bIsViewModelProperty ? ViewModelFieldDragDropOp->WidgetBP.Get() : WidgetPropertyDragDropOp->WidgetBP.Get();
		if (WidgetBlueprintPtr != DragDropWidgetBP)
		{
			return FReply::Unhandled();
		}

		TArray<FFieldVariant> FieldPath = bIsViewModelProperty ? ViewModelFieldDragDropOp->DraggedField : WidgetPropertyDragDropOp->DraggedPropertyPath;
		FMVVMBlueprintPropertyPath PropertyPath;
		for (const FFieldVariant& Field : FieldPath)
		{
			PropertyPath.AppendPropertyPath(WidgetBlueprintPtr, FMVVMConstFieldVariant(Field));
		}

		UMVVMEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();
		if (bIsViewModelProperty)
		{
			if (ViewModelFieldDragDropOp->ViewModelId.IsValid())
			{
				PropertyPath.SetViewModelId(ViewModelFieldDragDropOp->ViewModelId);
			}
			else
			{
				return FReply::Unhandled();
			}
		}
		else
		{
			if (UWidget* OwnerWidgetPtr = WidgetPropertyDragDropOp->OwnerWidget.Get())
			{
				// Search for the widget by its name in the widget tree
				// If the widget is not found, we know it is the root preview widget so we use the blueprint name.
				if (WidgetBlueprintPtr->WidgetTree->FindWidget(OwnerWidgetPtr->GetFName()) && WidgetBlueprintPtr->GetFName() != OwnerWidgetPtr->GetFName())
				{
					PropertyPath.SetWidgetName(OwnerWidgetPtr->GetFName());
				}
				else
				{
					PropertyPath.SetSelfContext();
				}
			}
		}

		if (bEvent)
		{
			Subsystem->SetEventPath(Event, PropertyPath);
		}
		else
		{
			Subsystem->SetEventDestinationPath(Event, PropertyPath);
		}
		return FReply::Handled();
	}

	void HandleFieldSelectorDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent, bool bEvent)
	{
		TSharedPtr<FDecoratedDragDropOp> DragDropOp = DragDropEvent.GetOperationAs<FDecoratedDragDropOp>();
		if (DragDropOp.IsValid())
		{
			// Accept all drag-drop operations that are widget properties, but only accept view model fields when we are dropping into the Source box.
			if (DragDropOp->IsOfType<FWidgetPropertyDragDropOp>() || (DragDropOp->IsOfType<FViewModelFieldDragDropOp>() && !bEvent))
			{
				if (UWidgetBlueprint* WidgetBlueprintPtr = WidgetBlueprintWeak.Get())
				{
					TSharedPtr<FViewModelFieldDragDropOp> ViewModelFieldDragDropOp = DragDropEvent.GetOperationAs<FViewModelFieldDragDropOp>();
					TSharedPtr<FWidgetPropertyDragDropOp> WidgetPropertyDragDropOp = DragDropEvent.GetOperationAs<FWidgetPropertyDragDropOp>();
					bool IsViewModelProperty = false;

					if (ViewModelFieldDragDropOp)
					{
						IsViewModelProperty = true;
					}

					UWidgetBlueprint* DragDropWidgetBP = IsViewModelProperty ? ViewModelFieldDragDropOp->WidgetBP.Get() : WidgetPropertyDragDropOp->WidgetBP.Get();

					if (DragDropWidgetBP && DragDropWidgetBP == WidgetBlueprintPtr)
					{
						DragDropOp->CurrentIconBrush = FAppStyle::GetBrush(TEXT("Graph.ConnectorFeedback.OK"));
						return;
					}
				}
			}
			else
			{
				DragDropOp->CurrentIconBrush = FAppStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error"));
			}
		}
	}

private:
	TSharedPtr<FBindingEntry> Entry;
	TWeakObjectPtr<UWidgetBlueprint> WidgetBlueprintWeak;
	TWeakObjectPtr<UMVVMBlueprintViewEvent> ViewEvent;
	TSharedPtr<SCustomDialog> ErrorDialog;
	TArray<TSharedPtr<FText>> ErrorItems;
};

/**
 * 
 */
class SFunctionParameterRow : public STableRow<TSharedPtr<FBindingEntry>>
{
	SLATE_BEGIN_ARGS(SFunctionParameterRow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& Args, const TSharedRef<STableViewBase>& OwnerTableView, const TSharedPtr<FBindingEntry>& InEntry, UWidgetBlueprint* InWidgetBlueprint)
	{
		Entry = InEntry;
		check(Entry->GetRowType() == FBindingEntry::ERowType::BindingParameter || Entry->GetRowType() == FBindingEntry::ERowType::EventParameter);

		WidgetBlueprint = InWidgetBlueprint;

		UMVVMEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();

		FSlateColor PrimaryColor, SecondaryColor;
		const FSlateBrush* PrimaryBrush = nullptr;
		const FSlateBrush* SecondaryBrush = nullptr;
		FText DisplayName, ToolTip;

		TSharedPtr<SWidget> ContentWidget;

		if (Entry->GetRowType() == FBindingEntry::ERowType::BindingParameter)
		{
			bool bSimpleConversionFunction = false;
			UMVVMBlueprintView* View = EditorSubsystem->GetView(InWidgetBlueprint);
			FMVVMBlueprintViewBinding* Binding = Entry->GetBinding(View);
			const bool bSourceToDestination = UE::MVVM::IsForwardBinding(Binding->BindingType);

			if (UEdGraphPin* Pin = EditorSubsystem->GetConversionFunctionArgumentPin(InWidgetBlueprint, *Binding, Entry->GetBindingParameterName(), bSourceToDestination))
			{
				PrimaryBrush = FBlueprintEditor::GetVarIconAndColorFromPinType(Pin->PinType, PrimaryColor, SecondaryBrush, SecondaryColor);
				DisplayName = Pin->GetDisplayName();
				ToolTip = FText::FromString(Pin->PinToolTip);
			}
			else if (const UFunction* Function = EditorSubsystem->GetConversionFunction(InWidgetBlueprint, *Binding, bSourceToDestination))
			{
				// no wrapper graph, this is a simple conversion function of the form: int32 Convert(float x)
				if (const FProperty* Argument = BindingHelper::GetFirstArgumentProperty(Function))
				{
					bSimpleConversionFunction = true;

					PrimaryBrush = FBlueprintEditor::GetVarIconAndColorFromProperty(Argument, PrimaryColor, SecondaryBrush, SecondaryColor);
					DisplayName = Argument->GetDisplayNameText();
					ToolTip = Argument->GetToolTipText();
				}
			}

			ContentWidget = SNew(SFunctionParameter, InWidgetBlueprint)
				.BindingId(Binding->BindingId)
				.ParameterName(Entry->GetBindingParameterName())
				.SourceToDestination(bSourceToDestination)
				.AllowDefault(!bSimpleConversionFunction);
		}
		else if (Entry->GetRowType() == FBindingEntry::ERowType::EventParameter)
		{
			UMVVMBlueprintViewEvent* ViewEvent = Entry->GetEvent();
			if (UEdGraphPin* Pin = ViewEvent ? ViewEvent->GetOrCreateGraphPin(Entry->GetEventParameterName()) : nullptr)
			{
				PrimaryBrush = FBlueprintEditor::GetVarIconAndColorFromPinType(Pin->PinType, PrimaryColor, SecondaryBrush, SecondaryColor);
				DisplayName = Pin->GetDisplayName();
				ToolTip = FText::FromString(Pin->PinToolTip);
			}

			ContentWidget = SNew(SEventParameter, InWidgetBlueprint)
				.Event(Entry->GetEvent())
				.ParameterName(Entry->GetEventParameterName())
				.AllowDefault(true);
		}

		STableRow<TSharedPtr<FBindingEntry>>::Construct(
			STableRow<TSharedPtr<FBindingEntry>>::FArguments()
			.ShowWires(true)
			.Style(FMVVMEditorStyle::Get(), "BindingView.ParameterRow")
			[
				SNew(SBox)
				.HeightOverride(30)
				.ToolTipText(ToolTip)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					.Padding(0, 0, 8, 0)
					.AutoWidth()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.Padding(0, 0, 8, 0)
						.AutoWidth()
						[
							SNew(SImage)
							.Image(PrimaryBrush)
							.ColorAndOpacity(PrimaryColor)
							.DesiredSizeOverride(FVector2D(16, 16))
						]
						+ SHorizontalBox::Slot()
						[
							SNew(STextBlock)
							.Text(DisplayName)
						]
					]
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						ContentWidget.ToSharedRef()
					]
				]
			], 
			OwnerTableView
		);
	}

private:
	TWeakObjectPtr<UWidgetBlueprint> WidgetBlueprint;
	TSharedPtr<FBindingEntry> Entry;
};

void SBindingsList::Construct(const FArguments& InArgs, TSharedPtr<SBindingsPanel> Owner, UMVVMWidgetBlueprintExtension_View* InMVVMExtension)
{
	BindingPanel = Owner;
	MVVMExtension = InMVVMExtension;
	check(InMVVMExtension);
	check(InMVVMExtension->GetBlueprintView());

	MVVMExtension->OnBlueprintViewChangedDelegate().AddSP(this, &SBindingsList::Refresh);
	MVVMExtension->GetBlueprintView()->OnBindingsUpdated.AddSP(this, &SBindingsList::Refresh);
	MVVMExtension->GetBlueprintView()->OnEventsUpdated.AddSP(this, &SBindingsList::Refresh);
	MVVMExtension->GetBlueprintView()->OnBindingsAdded.AddSP(this, &SBindingsList::ClearFilterText);
	MVVMExtension->GetBlueprintView()->OnViewModelsUpdated.AddSP(this, &SBindingsList::ForceRefresh);

	ChildSlot
	[
		SAssignNew(TreeView, STreeView<TSharedPtr<FBindingEntry>>)
		.TreeItemsSource(&FilteredRootGroups)
		.SelectionMode(ESelectionMode::Single)
		.OnGenerateRow(this, &SBindingsList::GenerateEntryRow)
		.OnGetChildren(this, &SBindingsList::GetChildrenOfEntry)
		.OnContextMenuOpening(this, &SBindingsList::OnSourceConstructContextMenu)
		.OnSelectionChanged(this, &SBindingsList::OnSourceListSelectionChanged)
		.ItemHeight(32)
	];

	Refresh();
}

SBindingsList::~SBindingsList()
{
	if (UMVVMWidgetBlueprintExtension_View* MVVMExtensionPtr = MVVMExtension.Get())
	{
		MVVMExtensionPtr->OnBlueprintViewChangedDelegate().RemoveAll(this);
		MVVMExtensionPtr->GetBlueprintView()->OnBindingsUpdated.RemoveAll(this);
		MVVMExtensionPtr->GetBlueprintView()->OnEventsUpdated.RemoveAll(this);
		MVVMExtensionPtr->GetBlueprintView()->OnBindingsAdded.RemoveAll(this);
		MVVMExtensionPtr->GetBlueprintView()->OnViewModelsUpdated.RemoveAll(this);
	}
}

void SBindingsList::GetChildrenOfEntry(TSharedPtr<FBindingEntry> Entry, TArray<TSharedPtr<FBindingEntry>>& OutChildren) const
{
	OutChildren.Append(Entry->GetFilteredChildren());
}

void SBindingsList::Refresh()
{
	struct FPreviousGroup
	{
		TSharedPtr<FBindingEntry> Group;
		TArray<TSharedPtr<FBindingEntry>> Children;
	};

	TArray<FPreviousGroup> PreviousRootGroups;
	for (const TSharedPtr<FBindingEntry>& PreviousEntry : AllRootGroups)
	{
		ensure(PreviousEntry->GetRowType() == FBindingEntry::ERowType::Group);
		FPreviousGroup& NewItem = PreviousRootGroups.AddDefaulted_GetRef();
		NewItem.Group = PreviousEntry;

		struct FRecursiveAdd
		{
			void operator()(FPreviousGroup& NewItem, const TSharedPtr<FBindingEntry>& Entry)
			{
				for (TSharedPtr<FBindingEntry> PreviousChildEntry : Entry->GetAllChildren())
				{
					NewItem.Children.Add(PreviousChildEntry);
					(*this)(NewItem, PreviousChildEntry);
				}
				Entry->ResetChildren();
			}
		};
		FRecursiveAdd{}(NewItem, PreviousEntry);
	}

	AllRootGroups.Reset();
	FilteredRootGroups.Reset();

	TArray<TSharedPtr<FBindingEntry>> NewEntries;

	UMVVMWidgetBlueprintExtension_View* MVVMExtensionPtr = MVVMExtension.Get();
	UMVVMBlueprintView* BlueprintView = MVVMExtensionPtr ? MVVMExtensionPtr->GetBlueprintView() : nullptr;
	UWidgetBlueprint* WidgetBlueprint = MVVMExtensionPtr ? MVVMExtensionPtr->GetWidgetBlueprint() : nullptr;

	// generate our entries
	// for each widget with bindings, create an entry at the root level
	// then add all bindings that reference that widget as its children
	if (BlueprintView)
	{
		auto FindPreviousGroupEntry = [&PreviousRootGroups, Self = this](FName GroupName)
		{
			return PreviousRootGroups.FindByPredicate([GroupName](const FPreviousGroup& Other) { return Other.Group->GetGroupName() == GroupName; });
		};
		auto FindGroupEntry = [&NewEntries, Self = this](FPreviousGroup* PreviousGroupEntry, FName GroupName, FGuid ViewModelId)
		{
			TSharedPtr<FBindingEntry> GroupEntry;
			if (PreviousGroupEntry)
			{
				GroupEntry = PreviousGroupEntry->Group;
			}
			else if (TSharedPtr<FBindingEntry>* FoundGroup = NewEntries.FindByPredicate([GroupName](const TSharedPtr<FBindingEntry>& Other)
				{ return Other->GetGroupName() == GroupName && Other->GetRowType() == FBindingEntry::ERowType::Group; }))
			{
				GroupEntry = *FoundGroup;
			}

			if (!GroupEntry.IsValid())
			{
				GroupEntry = MakeShared<FBindingEntry>();
				GroupEntry->SetGroup(GroupName, ViewModelId);

				NewEntries.Add(GroupEntry);
			}
			Self->AllRootGroups.AddUnique(GroupEntry);
			return GroupEntry;
		};

		for (const FMVVMBlueprintViewBinding& Binding : BlueprintView->GetBindings())
		{
			// Make sure the graph for the bindings is generated
			if (Binding.Conversion.SourceToDestinationConversion)
			{
				Binding.Conversion.SourceToDestinationConversion->GetOrCreateWrapperGraph(WidgetBlueprint);
			}
			if (Binding.Conversion.DestinationToSourceConversion)
			{
				Binding.Conversion.DestinationToSourceConversion->GetOrCreateWrapperGraph(WidgetBlueprint);
			}
			
			FName GroupName;
			FGuid GroupViewModelId;
			switch (Binding.DestinationPath.GetSource(WidgetBlueprint))
			{
			case EMVVMBlueprintFieldPathSource::SelfContext:
				GroupName = WidgetBlueprint->GetFName();
				break;
			case EMVVMBlueprintFieldPathSource::Widget:
				GroupName = Binding.DestinationPath.GetWidgetName();
				break;
			case EMVVMBlueprintFieldPathSource::ViewModel:
				if (const FMVVMBlueprintViewModelContext* ViewModelContext = BlueprintView->FindViewModel(Binding.DestinationPath.GetViewModelId()))
				{
					GroupName = ViewModelContext->GetViewModelName();
					GroupViewModelId = ViewModelContext->GetViewModelId();
				}
				break;
			}

			// Find the group entry
			FPreviousGroup* PreviousGroupEntry = FindPreviousGroupEntry(GroupName);
			TSharedPtr<FBindingEntry> GroupEntry = FindGroupEntry(PreviousGroupEntry, GroupName, GroupViewModelId);

			// Create/Find the child entry
			TSharedPtr<FBindingEntry> BindingEntry;
			FGuid BindingId = Binding.BindingId;
			{
				if (PreviousGroupEntry)
				{
					if (TSharedPtr<FBindingEntry>* FoundBinding = PreviousGroupEntry->Children.FindByPredicate([BindingId](const TSharedPtr<FBindingEntry>& Other)
						{ return Other->GetBindingId() == BindingId && Other->GetRowType() == FBindingEntry::ERowType::Binding; }))
					{
						BindingEntry = *FoundBinding;
					}
				}

				if (!BindingEntry.IsValid())
				{
					BindingEntry = MakeShared<FBindingEntry>();
					BindingEntry->SetBindingId(BindingId);

					NewEntries.Add(BindingEntry);
				}
				GroupEntry->AddChild(BindingEntry);
			}

			// Create/Find entries for conversion function parameters
			if (UMVVMBlueprintViewConversionFunction* ConversionFunction = Binding.Conversion.GetConversionFunction(UE::MVVM::IsForwardBinding(Binding.BindingType)))
			{
				ConversionFunction->GetOrCreateWrapperGraph(MVVMExtensionPtr->GetWidgetBlueprint());
				if (ConversionFunction->NeedsWrapperGraph())
				{
					for (const FMVVMBlueprintPin& Pin : ConversionFunction->GetPins())
					{
						UEdGraphPin* GraphPin = ConversionFunction->GetOrCreateGraphPin(MVVMExtensionPtr->GetWidgetBlueprint(), Pin.GetName());
						if (GraphPin && GraphPin->bHidden)
						{
							continue;
						}

						TSharedPtr<FBindingEntry> ArgumentEntry;
						if (PreviousGroupEntry)
						{
							if (TSharedPtr<FBindingEntry>* FoundParameter = PreviousGroupEntry->Children.FindByPredicate([BindingId, ArgumentName = Pin.GetName()](const TSharedPtr<FBindingEntry>& Other)
								{ return Other->GetBindingId() == BindingId && Other->GetRowType() == FBindingEntry::ERowType::BindingParameter && Other->GetBindingParameterName() == ArgumentName; }))
							{
								ArgumentEntry = *FoundParameter;
							}
						}

						if (!ArgumentEntry.IsValid())
						{
							ArgumentEntry = MakeShared<FBindingEntry>();
							ArgumentEntry->SetBindingParameterName(Binding.BindingId, Pin.GetName());

							NewEntries.Add(ArgumentEntry);
						}
						BindingEntry->AddChild(ArgumentEntry);
					}
				}
				else // simple conversion function with one parameters (no subpins)
				{
					const UFunction* Function = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>()->GetConversionFunction(MVVMExtensionPtr->GetWidgetBlueprint(), Binding, UE::MVVM::IsForwardBinding(Binding.BindingType));
					if (Function != nullptr)
					{
						TValueOrError<TArray<const FProperty*>, FText> ArgumentsResult = BindingHelper::TryGetArgumentsForConversionFunction(Function);
						if (ArgumentsResult.HasValue())
						{
							for (const FProperty* Argument : ArgumentsResult.GetValue())
							{
								TSharedPtr<FBindingEntry> ArgumentEntry;
								if (PreviousGroupEntry)
								{
									if (TSharedPtr<FBindingEntry>* FoundParameter = PreviousGroupEntry->Children.FindByPredicate([BindingId, ArgumentName = Argument->GetFName()](const TSharedPtr<FBindingEntry>& Other)
										{ return Other->GetBindingId() == BindingId && Other->GetRowType() == FBindingEntry::ERowType::BindingParameter && Other->GetBindingParameterName() == ArgumentName; }))
									{
										ArgumentEntry = *FoundParameter;
									}
								}

								if (!ArgumentEntry.IsValid())
								{
									ArgumentEntry = MakeShared<FBindingEntry>();
									ArgumentEntry->SetBindingParameterName(Binding.BindingId, Argument->GetFName());

									NewEntries.Add(ArgumentEntry);
								}
								BindingEntry->AddChild(ArgumentEntry);
							}
						}
					}
				}
			}
		}

		for (UMVVMBlueprintViewEvent* Event : BlueprintView->GetEvents())
		{
			Event->GetOrCreateWrapperGraph();

			FName GroupName;
			FGuid GroupViewModelId;
			switch (Event->GetEventPath().GetSource(WidgetBlueprint))
			{
			case EMVVMBlueprintFieldPathSource::SelfContext:
				GroupName = WidgetBlueprint->GetFName();
				break;
			case EMVVMBlueprintFieldPathSource::Widget:
				GroupName = Event->GetEventPath().GetWidgetName();
				break;
			case EMVVMBlueprintFieldPathSource::ViewModel:
				if (const FMVVMBlueprintViewModelContext* ViewModelContext = BlueprintView->FindViewModel(Event->GetEventPath().GetViewModelId()))
				{
					GroupName = ViewModelContext->GetViewModelName();
					GroupViewModelId = ViewModelContext->GetViewModelId();
				}
				break;
			}

			// Find the group entry
			FPreviousGroup* PreviousGroupEntry = FindPreviousGroupEntry(GroupName);
			TSharedPtr<FBindingEntry> GroupEntry = FindGroupEntry(PreviousGroupEntry, GroupName, GroupViewModelId);

			// Create/Find the child entry
			TSharedPtr<FBindingEntry> EventEntry;
			{
				if (PreviousGroupEntry)
				{
					if (TSharedPtr<FBindingEntry>* FoundBinding = PreviousGroupEntry->Children.FindByPredicate([Event](const TSharedPtr<FBindingEntry>& Other)
						{
							return Other->GetRowType() == FBindingEntry::ERowType::Event && Other->GetEvent() == Event;
						}))
					{
						EventEntry = *FoundBinding;
					}
				}

				if (!EventEntry.IsValid())
				{
					EventEntry = MakeShared<FBindingEntry>();
					EventEntry->SetEvent(Event);

					NewEntries.Add(EventEntry);
				}
				GroupEntry->AddChild(EventEntry);
			}

			// Create/Find entries for function parameters
			for (const FMVVMBlueprintPin& Pin : Event->GetPins())
			{
				UEdGraphPin* GraphPin = Event->GetOrCreateGraphPin(Pin.GetName());
				if (GraphPin && GraphPin->bHidden)
				{
					continue;
				}

				TSharedPtr<FBindingEntry> ArgumentEntry;
				if (PreviousGroupEntry)
				{
					if (TSharedPtr<FBindingEntry>* FoundParameter = PreviousGroupEntry->Children.FindByPredicate([Event, ArgumentName = Pin.GetName()](const TSharedPtr<FBindingEntry>& Other)
						{ return Other->GetRowType() == FBindingEntry::ERowType::EventParameter && Other->GetEvent() == Event && Other->GetEventParameterName() == ArgumentName; }))
					{
						ArgumentEntry = *FoundParameter;
					}
				}

				if (!ArgumentEntry.IsValid())
				{
					ArgumentEntry = MakeShared<FBindingEntry>();
					ArgumentEntry->SetEventParameterName(Event, Pin.GetName());

					NewEntries.Add(ArgumentEntry);
				}
				EventEntry->AddChild(ArgumentEntry);
			}
		}

		Private::FilterEntryList(FilterText.ToString(), AllRootGroups, FilteredRootGroups, BlueprintView, MVVMExtensionPtr);
	}

	if (TreeView.IsValid())
	{
		TreeView->RequestTreeRefresh();
		for (const TSharedPtr<FBindingEntry>& Entry : NewEntries)
		{
			Private::ExpandAll(TreeView, Entry);
		}
	}
}

void SBindingsList::ForceRefresh()
{
	AllRootGroups.Reset();
	FilteredRootGroups.Reset();
	Refresh();
}

TSharedRef<ITableRow> SBindingsList::GenerateEntryRow(TSharedPtr<FBindingEntry> Entry, const TSharedRef<STableViewBase>& OwnerTable) const
{
	TSharedPtr<ITableRow> Row;

	if (UMVVMWidgetBlueprintExtension_View* MVVMExtensionPtr = MVVMExtension.Get())
	{
		switch (Entry->GetRowType())
		{
			case FBindingEntry::ERowType::Group:
			{
				Row = SNew(SGroupRow, OwnerTable, Entry, MVVMExtensionPtr->GetWidgetBlueprint());
				break;
			}
			case FBindingEntry::ERowType::Binding:
			{
				Row = SNew(SBindingRow, OwnerTable, Entry, MVVMExtensionPtr->GetWidgetBlueprint());
				break;
			}
			case FBindingEntry::ERowType::BindingParameter:
			case FBindingEntry::ERowType::EventParameter:
			{
				Row = SNew(SFunctionParameterRow, OwnerTable, Entry, MVVMExtensionPtr->GetWidgetBlueprint());
				break;
			}
			case FBindingEntry::ERowType::Event:
			{
				Row = SNew(SEventRow, OwnerTable, Entry, MVVMExtensionPtr->GetWidgetBlueprint());
				break;
			}
		}

		return Row.ToSharedRef();
	}

	ensureMsgf(false, TEXT("Failed to create binding or widget row."));
	return SNew(STableRow<TSharedPtr<FBindingEntry>>, OwnerTable);
}

void SBindingsList::OnFilterTextChanged(const FText& InFilterText)
{
	FilterText = InFilterText;
	Refresh();
}

void SBindingsList::ClearFilterText()
{
	FilterText = FText::GetEmpty();
}

namespace Private
{
	void GatherAllChildBindings(UMVVMBlueprintView* BlueprintView, const TConstArrayView<TSharedPtr<FBindingEntry>>& Entries, TArray<const FMVVMBlueprintViewBinding*>& OutBindings, TArray<UMVVMBlueprintViewEvent*> & OutEvents)
	{
		for (const TSharedPtr<FBindingEntry>& Entry : Entries)
		{
			if (Entry->GetRowType() == FBindingEntry::ERowType::Binding)
			{
				const FMVVMBlueprintViewBinding* Binding = Entry->GetBinding(BlueprintView);
				if (Binding != nullptr)
				{
					OutBindings.AddUnique(Binding);
				}
			}

			if (Entry->GetRowType() == FBindingEntry::ERowType::Event)
			{
				UMVVMBlueprintViewEvent* Event = Entry->GetEvent();
				if (Event)
				{
					OutEvents.AddUnique(Event);
				}
			}
			GatherAllChildBindings(BlueprintView, Entry->GetAllChildren(), OutBindings, OutEvents);
		}
	}
} //namespace

void SBindingsList::HandleDeleteSelected()
{
	TArray<TSharedPtr<FBindingEntry>> Selection = TreeView->GetSelectedItems();
	if (Selection.Num() == 0)
	{
		return;
	}

	if (UMVVMWidgetBlueprintExtension_View* MVVMExtensionPtr = MVVMExtension.Get())
	{
		if (UMVVMBlueprintView* BlueprintView = MVVMExtensionPtr->GetBlueprintView())
		{
			const UWidgetBlueprint* WidgetBlueprint = MVVMExtensionPtr->GetWidgetBlueprint();

			TArray<const FMVVMBlueprintViewBinding*> BindingsToRemove;
			TArray<UMVVMBlueprintViewEvent*> EventsToRemove;
			Private::GatherAllChildBindings(BlueprintView, Selection, BindingsToRemove, EventsToRemove);

			if (BindingsToRemove.Num() == 0 && EventsToRemove.Num() == 0)
			{
				return;
			}

			TArray<FText> BindingDisplayNames;
			for (const FMVVMBlueprintViewBinding* Binding : BindingsToRemove)
			{
				BindingDisplayNames.Add(FText::FromString(Binding->GetDisplayNameString(WidgetBlueprint)));
			}
			for (const UMVVMBlueprintViewEvent* Event : EventsToRemove)
			{
				BindingDisplayNames.Add(Event->GetDisplayName(true));
			}

			const FText Message = FText::Format(BindingDisplayNames.Num() == 1 ?
				LOCTEXT("ConfirmDeleteSingle", "Are you sure that you want to delete this binding?\n\n{1}") :
				LOCTEXT("ConfirmDeleteMultiple", "Are you sure that you want to delete these {0} bindings?\n\n{1}"), 
			BindingDisplayNames.Num(), 
			FText::Join(FText::FromString("\n"), BindingDisplayNames));

			const FText Title = LOCTEXT("DeleteBindings", "Delete Bindings?");

			if (FMessageDialog::Open(EAppMsgType::YesNo, EAppReturnType::Yes, Message, Title) == EAppReturnType::Yes)
			{
				FScopedTransaction Transaction(LOCTEXT("DeleteBindingsTransaction", "Delete Bindings"));

				if (TSharedPtr<SBindingsPanel> BindingPanelPtr = BindingPanel.Pin())
				{
					BindingPanelPtr->OnBindingListSelectionChanged(TConstArrayView<FMVVMBlueprintViewBinding*>());
				}

				BlueprintView->Modify();

				for (const FMVVMBlueprintViewBinding* Binding : BindingsToRemove)
				{
					BlueprintView->RemoveBinding(Binding);
				}
				for (UMVVMBlueprintViewEvent* Event : EventsToRemove)
				{
					BlueprintView->RemoveEvent(Event);
				}
			}
		}
	}
}

void SBindingsList::HandleBreakSelectedPin()
{
	if (UMVVMWidgetBlueprintExtension_View* MVVMExtensionPtr = MVVMExtension.Get())
	{
		UWidgetBlueprint* WidgetBlueprint = MVVMExtensionPtr->GetWidgetBlueprint();
		check(WidgetBlueprint);
		UMVVMEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();

		TArray<TSharedPtr<FBindingEntry>> Selection = TreeView->GetSelectedItems();
		for (const TSharedPtr<FBindingEntry>& Entry : Selection)
		{
			if (Entry->GetRowType() == FBindingEntry::ERowType::EventParameter)
			{
				EditorSubsystem->SplitPin(WidgetBlueprint, Entry->GetEvent(), Entry->GetEventParameterName());
			}
			else if(Entry->GetRowType() == FBindingEntry::ERowType::BindingParameter)
			{
				if (FMVVMBlueprintViewBinding* Binding = Entry->GetBinding(MVVMExtensionPtr->GetBlueprintView()))
				{
					const bool bSourceToDestination = UE::MVVM::IsForwardBinding(Binding->BindingType);
					EditorSubsystem->SplitPin(WidgetBlueprint, *Binding, Entry->GetEventParameterName(), bSourceToDestination);
				}
			}
		}
	}
}

void SBindingsList::HandleRecombineSelectedPin()
{
	if (UMVVMWidgetBlueprintExtension_View* MVVMExtensionPtr = MVVMExtension.Get())
	{
		const FScopedTransaction Transaction(LOCTEXT("RecombineStructPin", "Recombine Struct Pin"));

		UWidgetBlueprint* WidgetBlueprint = MVVMExtensionPtr->GetWidgetBlueprint();
		check(WidgetBlueprint);
		UMVVMEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();

		TArray<TSharedPtr<FBindingEntry>> Selection = TreeView->GetSelectedItems();
		for (const TSharedPtr<FBindingEntry>& Entry : Selection)
		{
			if (Entry->GetRowType() == FBindingEntry::ERowType::EventParameter)
			{
				EditorSubsystem->RecombinePin(WidgetBlueprint, Entry->GetEvent(), Entry->GetEventParameterName());
			}
			else if (Entry->GetRowType() == FBindingEntry::ERowType::BindingParameter)
			{
				if (FMVVMBlueprintViewBinding* Binding = Entry->GetBinding(MVVMExtensionPtr->GetBlueprintView()))
				{
					const bool bSourceToDestination = UE::MVVM::IsForwardBinding(Binding->BindingType);
					EditorSubsystem->RecombinePin(WidgetBlueprint, *Binding, Entry->GetEventParameterName(), bSourceToDestination);
				}
			}
		}
	}
}

TSharedPtr<SWidget> SBindingsList::OnSourceConstructContextMenu()
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, nullptr);

	TArray<TSharedPtr<FBindingEntry>> Selection = TreeView->GetSelectedItems();
	if (Selection.Num() > 0)
	{
		{
			FUIAction RemoveAction;
			RemoveAction.ExecuteAction = FExecuteAction::CreateSP(this, &SBindingsList::HandleDeleteSelected);
			MenuBuilder.AddMenuEntry(LOCTEXT("RemoveBinding", "Remove Binding"),
				LOCTEXT("RemoveBindingTooltip", "Remove this binding."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Delete"),
				RemoveAction);
		}

		{
			const UMVVMWidgetBlueprintExtension_View* MVVMExtensionPtr = MVVMExtension.Get();
			const UWidgetBlueprint* WidgetBlueprint = MVVMExtensionPtr ? MVVMExtensionPtr->GetWidgetBlueprint() : nullptr;
			const UMVVMBlueprintView* View = MVVMExtensionPtr ? MVVMExtensionPtr->GetBlueprintView() : nullptr;
			const UMVVMEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UMVVMEditorSubsystem>();

			if (View && WidgetBlueprint)
			{
				bool bCanSplitPin = true;
				bool bCanRecombinePin = true;
				bool bCanRecombinePinVisible = true;
				for (const TSharedPtr<FBindingEntry>& Entry : Selection)
				{
					if (Entry->GetRowType() == FBindingEntry::ERowType::EventParameter)
					{
						if (!EditorSubsystem->CanSplitPin(WidgetBlueprint, Entry->GetEvent(), Entry->GetEventParameterName()))
						{
							bCanSplitPin = false;
						}
						if (!EditorSubsystem->CanRecombinePin(WidgetBlueprint, Entry->GetEvent(), Entry->GetEventParameterName()))
						{
							bCanRecombinePin = false;
						}
						UMVVMBlueprintViewEvent* ViewEvent = Entry->GetEvent();
						UEdGraphPin* GraphPin = ViewEvent ? ViewEvent->GetOrCreateGraphPin(Entry->GetEventParameterName()) : nullptr;
						if (GraphPin == nullptr || GraphPin->ParentPin == nullptr)
						{
							bCanRecombinePinVisible = false;
						}
					}
					else if (Entry->GetRowType() == FBindingEntry::ERowType::BindingParameter)
					{
						const FMVVMBlueprintViewBinding* Binding = Entry->GetBinding(View);
						if (Binding)
						{
							const bool bSourceToDestination = UE::MVVM::IsForwardBinding(Binding->BindingType);
							if (!EditorSubsystem->CanSplitPin(WidgetBlueprint, *Binding, Entry->GetBindingParameterName(), bSourceToDestination))
							{
								bCanSplitPin = false;
							}
							if (!EditorSubsystem->CanRecombinePin(WidgetBlueprint, *Binding, Entry->GetBindingParameterName(), bSourceToDestination))
							{
								bCanRecombinePin = false;
							}

							UEdGraphPin* GraphPin = GraphPin = EditorSubsystem->GetConversionFunctionArgumentPin(WidgetBlueprint, *Binding, Entry->GetBindingParameterName(), bSourceToDestination);
							if (GraphPin == nullptr || GraphPin->ParentPin == nullptr)
							{
								bCanRecombinePinVisible = false;
							}
						}
						else
						{
							bCanSplitPin = false;
							bCanRecombinePin = false;
							bCanRecombinePinVisible = false;
						}
					}

					if (!bCanRecombinePin && !bCanSplitPin && !bCanRecombinePinVisible)
					{
						break;
					}
				}

				if (bCanSplitPin)
				{
					FUIAction SplitPinAction;
					SplitPinAction.ExecuteAction = FExecuteAction::CreateSP(this, &SBindingsList::HandleBreakSelectedPin);
					MenuBuilder.AddMenuEntry(LOCTEXT("BreakPin", "Split Struct Pin"),
						LOCTEXT("BreakPinTooltip", "Breaks a struct pin in to a separate pin per element."),
						FSlateIcon(),
						SplitPinAction);
				}
				if (bCanRecombinePinVisible)
				{
					FUIAction RecombinePinAction;
					RecombinePinAction.ExecuteAction = FExecuteAction::CreateSP(this, &SBindingsList::HandleRecombineSelectedPin);
					RecombinePinAction.CanExecuteAction = FCanExecuteAction::CreateLambda([bCanRecombinePin](){ return bCanRecombinePin; });
					MenuBuilder.AddMenuEntry(LOCTEXT("RecombinePin", "Recombine Struct Pin"),
						LOCTEXT("RecombinePinTooltip", "Takes struct pins that have been broken in to composite elements and combines them back to a single struct pin."),
						FSlateIcon(),
						RecombinePinAction);
				}
			}
		}
	}

	return MenuBuilder.MakeWidget();
}

void SBindingsList::RequestNavigateToBinding(FGuid BindingId)
{
	TSharedPtr<FBindingEntry> Entry = Private::FindBinding(BindingId, FilteredRootGroups);
	if (Entry && TreeView)
	{
		TreeView->RequestNavigateToItem(Entry);
	}
}

void SBindingsList::RequestNavigateToEvent(UMVVMBlueprintViewEvent* Event)
{
	TSharedPtr<FBindingEntry> Entry = Private::FindEvent(Event, FilteredRootGroups);
	if (Entry && TreeView)
	{
		TreeView->RequestNavigateToItem(Entry);
	}
}

FReply SBindingsList::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Delete)
	{
		HandleDeleteSelected();
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SBindingsList::OnSourceListSelectionChanged(TSharedPtr<FBindingEntry> Entry, ESelectInfo::Type SelectionType) const
{
	if (bSelectionChangedGuard)
	{
		return;
	}
	TGuardValue<bool> ReentrantGuard(bSelectionChangedGuard, true);

	if (TSharedPtr<SBindingsPanel> BindingPanelPtr = BindingPanel.Pin())
	{
		if (UMVVMWidgetBlueprintExtension_View* MVVMExtensionPtr = MVVMExtension.Get())
		{
			if (UMVVMBlueprintView* View = MVVMExtensionPtr->GetBlueprintView())
			{
				TArray<TSharedPtr<FBindingEntry>> SelectedEntries = TreeView->GetSelectedItems();
				TArray<FMVVMBlueprintViewBinding*> SelectedBindings;

				for (const TSharedPtr<FBindingEntry>& SelectedEntry : SelectedEntries)
				{
					if (FMVVMBlueprintViewBinding* SelectedBinding = Entry->GetBinding(View))
					{
						SelectedBindings.Add(SelectedBinding);
					}
				}

				BindingPanelPtr->OnBindingListSelectionChanged(SelectedBindings);
			}
		}
	}
}

} // namespace UE::MVVM

#undef LOCTEXT_NAMESPACE
