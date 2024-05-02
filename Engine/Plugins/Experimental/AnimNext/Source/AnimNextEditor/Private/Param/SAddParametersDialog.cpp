// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAddParametersDialog.h"

#include "AddParameterDialogMenuContext.h"
#include "AnimNextParameterSettings.h"
#include "AssetToolsModule.h"
#include "ContentBrowserModule.h"
#include "DetailLayoutBuilder.h"
#include "IContentBrowserSingleton.h"
#include "EditorUtils.h"
#include "UncookedOnlyUtils.h"
#include "PropertyBagDetails.h"
#include "SParameterPickerCombo.h"
#include "SPinTypeSelector.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "SSimpleButton.h"
#include "SSimpleComboButton.h"
#include "ToolMenus.h"
#include "String/ParseTokens.h"

#define LOCTEXT_NAMESPACE "SAddParametersDialog"

namespace UE::AnimNext::Editor
{

namespace AddParametersDialog
{
static FName Column_Name(TEXT("Name"));
static FName Column_Type(TEXT("Type"));
static FName SelectLibraryMenuName(TEXT("AnimNext.AddParametersDialog.SelectedLibraryMenu"));
}

bool FParameterToAdd::IsValid(FText& OutReason) const
{
	if(Name == NAME_None)
	{
		OutReason = LOCTEXT("InvalidParameterName", "Invalid Parameter Name");
	}

	if(!Type.IsValid())
	{
		OutReason = LOCTEXT("InvalidParameterType", "Invalid Parameter Type");
	}
	
	return true; 
}

void SAddParametersDialog::Construct(const FArguments& InArgs, const FAssetData& InAsset)
{
	using namespace AddParametersDialog;

	OnFilterParameterType = InArgs._OnFilterParameterType;
	Asset = InAsset;

	SWindow::Construct(SWindow::FArguments()
		.Title(LOCTEXT("WindowTitle", "Add Parameters"))
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(InArgs._AllowMultiple ? FVector2D(500.f, 500.f) : FVector2D(500.f, 100.f))
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		[
			SNew(SBox)
			.Padding(5.0f)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Left)
				.Padding(0.0f, 5.0f)
				[
					SNew(SSimpleButton)
					.Visibility(InArgs._AllowMultiple ? EVisibility::Visible : EVisibility::Collapsed)
					.Text(LOCTEXT("AddButton", "Add"))
					.ToolTipText(LOCTEXT("AddButtonTooltip", "Queue a new parameter for adding. New parameters will re-use the settings from the last queued parameter."))
					.Icon(FAppStyle::Get().GetBrush("Icons.Plus"))
					.OnClicked_Lambda([this]()
					{
						AddEntry();
						return FReply::Handled();
					})
				]
				+SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					SAssignNew(EntriesList, SListView<TSharedRef<FParameterToAddEntry>>)
					.ListItemsSource(&Entries)
					.OnGenerateRow(this, &SAddParametersDialog::HandleGenerateRow)
					.HeaderRow(
						SNew(SHeaderRow)
						+SHeaderRow::Column(Column_Name)
						.DefaultLabel(LOCTEXT("NameColumnHeader", "Name"))
						.ToolTipText(LOCTEXT("NameColumnHeaderTooltip", "The name of the new parameter"))
						.FillWidth(0.25f)

						+SHeaderRow::Column(Column_Type)
						.DefaultLabel(LOCTEXT("TypeColumnHeader", "Type"))
						.ToolTipText(LOCTEXT("TypeColumnHeaderTooltip", "The type of the new parameter"))
						.FillWidth(0.25f)
					)
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Right)
				[
					SNew(SUniformGridPanel)
					.SlotPadding(FAppStyle::Get().GetMargin("StandardDialog.SlotPadding"))
					.MinDesiredSlotWidth(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
					.MinDesiredSlotHeight(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
					+SUniformGridPanel::Slot(0, 0)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("PrimaryButton"))
						.IsEnabled_Lambda([this]()
						{
							// Check each entry to see if the button can be pressed
							for(TSharedRef<FParameterToAdd> Entry : Entries)
							{
								if(!Entry->IsValid())
								{
									return false;
								}
							}

							return true;
						})
						.Text_Lambda([this]()
						{
							return FText::Format(LOCTEXT("AddParametersButtonFormat", "Add {0} {0}|plural(one=Parameter,other=Parameters)"), FText::AsNumber(Entries.Num()));
						})
						.ToolTipText_Lambda([this]()
						{
							// Check each entry to see if the button can be pressed
							for(TSharedRef<FParameterToAdd> Entry : Entries)
							{
								FText Reason;
								if(!Entry->IsValid(Reason))
								{
									return FText::Format(LOCTEXT("AddParametersButtonTooltip_InvalidEntry", "A parameter to add is not valid: {0}"), Reason);
								}
							}
							return LOCTEXT("AddParametersButtonTooltip", "Add the selected parameters to the current graph");
						})
						.OnClicked_Lambda([this]()
						{
							bOKPressed = true;
							RequestDestroyWindow();
							return FReply::Handled();
						})
					]
					+SUniformGridPanel::Slot(1, 0)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
						.Text(LOCTEXT("CancelButton", "Cancel"))
						.ToolTipText(LOCTEXT("CancelButtonTooltip", "Cancel adding new parameters"))
						.OnClicked_Lambda([this]()
						{
							RequestDestroyWindow();
							return FReply::Handled();
						})
					]
				]
			]
		]);

	// Add an initial item
	AddEntry(InArgs._InitialParamType);
}

FReply SAddParametersDialog::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if(InKeyEvent.GetKey() == EKeys::Escape)
	{
		RequestDestroyWindow();
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SAddParametersDialog::AddEntry(const FAnimNextParamType& InParamType)
{
	const UAnimNextParameterSettings* Settings = GetDefault<UAnimNextParameterSettings>();
	
	TArray<FName> PendingNames;
	PendingNames.Reserve(Entries.Num());
	for(const TSharedRef<FParameterToAddEntry>& QueuedAdd : Entries)
	{
		PendingNames.Add(QueuedAdd->Name);
	}
	FName ParameterName = FUtils::GetNewParameterName(Settings->GetLastParameterName(), Asset, PendingNames);
	Entries.Add(MakeShared<FParameterToAddEntry>(InParamType.IsValid() ? InParamType : Settings->GetLastParameterType(), ParameterName));

	RefreshEntries();
}

void SAddParametersDialog::RefreshEntries()
{
	EntriesList->RequestListRefresh();
}

class SParameterToAdd : public SMultiColumnTableRow<TSharedRef<SAddParametersDialog::FParameterToAddEntry>>
{
	SLATE_BEGIN_ARGS(SParameterToAdd) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, TSharedRef<SAddParametersDialog::FParameterToAddEntry> InEntry, TSharedRef<SAddParametersDialog> InDialog)
	{
		Entry = InEntry;
		WeakDialog = InDialog;
		
		SMultiColumnTableRow<TSharedRef<SAddParametersDialog::FParameterToAddEntry>>::Construct( SMultiColumnTableRow<TSharedRef<SAddParametersDialog::FParameterToAddEntry>>::FArguments(), InOwnerTableView);
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& InColumnName) override
	{
		using namespace AddParametersDialog;

		if(InColumnName == Column_Name)
		{
			TSharedPtr<SInlineEditableTextBlock> EditableText;
			TSharedRef<SWidget> Widget =
				SNew(SBox)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SAssignNew(EditableText, SInlineEditableTextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.IsSelected(this, &SParameterToAdd::IsSelectedExclusively)
					.ToolTipText(LOCTEXT("NameTooltip", "The name of the new parameter"))
					.Text_Lambda([this]()
					{
						return FText::FromName(Entry->Name);
					})
					.OnTextCommitted_Lambda([this](const FText& InText, ETextCommit::Type InCommitType)
					{
						Entry->Name = *InText.ToString();

						UAnimNextParameterSettings* Settings = GetMutableDefault<UAnimNextParameterSettings>();
						Settings->SetLastParameterName(Entry->Name);
					})
					.OnVerifyTextChanged_Lambda([this](const FText& InNewText, FText& OutErrorText)
					{
						const FString NewString = InNewText.ToString();

						if(!FUtils::IsValidParameterNameString(NewString, OutErrorText))
						{
							return false;
						}

						if(TSharedPtr<SAddParametersDialog> Dialog = WeakDialog.Pin())
						{
							const FName Name(*NewString);
							if(FUtils::DoesParameterNameExistInAsset(Name, Dialog->Asset))
							{
								OutErrorText = LOCTEXT("Error_NameExists", "This name already exists in the project");
								return false;
							}

							return true;
						}

						return false;
					})
				];

			if(Entry->bIsNew)
			{
				EditableText->RegisterActiveTimer(1/60.0f, FWidgetActiveTimerDelegate::CreateSPLambda(EditableText.Get(), [WeakEditableText = TWeakPtr<SInlineEditableTextBlock>(EditableText)](double, float)
				{
					if(TSharedPtr<SInlineEditableTextBlock> PinnedEditableText = WeakEditableText.Pin())
					{
						PinnedEditableText->EnterEditingMode();
					}
					return EActiveTimerReturnType::Stop;
				}));
	
				Entry->bIsNew = false;
			}

			return Widget;
		}
		else if(InColumnName == Column_Type)
		{
			auto GetPinInfo = [this]()
			{
				return UncookedOnly::FUtils::GetPinTypeFromParamType(Entry->Type);
			};

			auto PinInfoChanged = [this](const FEdGraphPinType& PinType)
			{
				Entry->Type = UncookedOnly::FUtils::GetParamTypeFromPinType(PinType);

				UAnimNextParameterSettings* Settings = GetMutableDefault<UAnimNextParameterSettings>();
				Settings->SetLastParameterType(Entry->Type);
			};
			
			auto GetFilteredVariableTypeTree = [this](TArray<TSharedPtr<UEdGraphSchema_K2::FPinTypeTreeInfo>>& TypeTree, ETypeTreeFilter TypeTreeFilter)
			{
				FUtils::GetFilteredVariableTypeTree(TypeTree, TypeTreeFilter);

				if(TSharedPtr<SAddParametersDialog> Dialog = WeakDialog.Pin())
				{
					if(Dialog->OnFilterParameterType.IsBound())
					{
						auto IsPinTypeAllowed = [&Dialog](const FEdGraphPinType& InType)
						{
							FAnimNextParamType Type = UncookedOnly::FUtils::GetParamTypeFromPinType(InType);
							if(Type.IsValid())
							{
								return Dialog->OnFilterParameterType.Execute(Type) == EFilterParameterResult::Include;
							}
							return false;
						};

						// Additionally filter by allowed types
						for (int32 Index = 0; Index < TypeTree.Num(); )
						{
							TSharedPtr<UEdGraphSchema_K2::FPinTypeTreeInfo>& PinType = TypeTree[Index];

							if (PinType->Children.Num() == 0 && !IsPinTypeAllowed(PinType->GetPinType(/*bForceLoadSubCategoryObject*/false)))
							{
								TypeTree.RemoveAt(Index);
								continue;
							}

							for (int32 ChildIndex = 0; ChildIndex < PinType->Children.Num(); )
							{
								TSharedPtr<UEdGraphSchema_K2::FPinTypeTreeInfo> Child = PinType->Children[ChildIndex];
								if (Child.IsValid())
								{
									if (!IsPinTypeAllowed(Child->GetPinType(/*bForceLoadSubCategoryObject*/false)))
									{
										PinType->Children.RemoveAt(ChildIndex);
										continue;
									}
								}
								++ChildIndex;
							}

							++Index;
						}
					}
				}
			};

			return
				SNew(SBox)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SNew(SPinTypeSelector, FGetPinTypeTree::CreateLambda(GetFilteredVariableTypeTree))
						.TargetPinType_Lambda(GetPinInfo)
						.OnPinTypeChanged_Lambda(PinInfoChanged)
						.Schema(GetDefault<UPropertyBagSchema>())
						.bAllowArrays(true)
						.TypeTreeFilter(ETypeTreeFilter::None)
						.Font(IDetailLayoutBuilder::GetDetailFont())
				];
		}

		return SNullWidget::NullWidget;
	}

	TSharedPtr<SAddParametersDialog::FParameterToAddEntry> Entry;
	TWeakPtr<SAddParametersDialog> WeakDialog;
};

TSharedRef<ITableRow> SAddParametersDialog::HandleGenerateRow(TSharedRef<FParameterToAddEntry> InEntry, const TSharedRef<STableViewBase>& InOwnerTable)
{
	return SNew(SParameterToAdd, InOwnerTable, InEntry, SharedThis(this));
}

bool SAddParametersDialog::ShowModal(TArray<FParameterToAdd>& OutParameters)
{
	FSlateApplication::Get().AddModalWindow(SharedThis(this), FGlobalTabmanager::Get()->GetRootWindow());

	if(bOKPressed)
	{
		bool bHasValid = false;
		for(TSharedRef<FParameterToAddEntry>& Entry : Entries)
		{
			if(Entry->IsValid())
			{
				OutParameters.Add(*Entry);
				bHasValid = true;
			}
		}
		return bHasValid;
	}
	return false;
}

TSharedRef<SWidget> SAddParametersDialog::HandleGetAddParameterMenuContent(TSharedPtr<FParameterToAddEntry> InEntry)
{
	using namespace AddParametersDialog;

	UToolMenus* ToolMenus = UToolMenus::Get();

	UAddParameterDialogMenuContext* MenuContext = NewObject<UAddParameterDialogMenuContext>();
	MenuContext->AddParametersDialog = SharedThis(this);
	MenuContext->Entry = InEntry;
	return ToolMenus->GenerateWidget(SelectLibraryMenuName, FToolMenuContext(MenuContext));
}

}

#undef LOCTEXT_NAMESPACE