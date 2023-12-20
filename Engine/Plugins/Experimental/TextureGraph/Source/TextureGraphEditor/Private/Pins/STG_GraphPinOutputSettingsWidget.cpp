// Copyright Epic Games, Inc. All Rights Reserved.

#include "STG_GraphPinOutputSettingsWidget.h"
#include "DetailLayoutBuilder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SComboButton.h"
#include "ScopedTransaction.h"
#include "Editor.h"
#include "Widgets/Layout/SWrapBox.h"
#include "SGraphPinComboBox.h"
#include "FrameWork/MultiBox/MultiBoxBuilder.h"
#include "Customizations/TG_OutputSettingsCustomization.h"

#define LOCTEXT_NAMESPACE "STG_GraphPinOutputSettingsWidget"

//------------------------------------------------------------------------------
// STG_GraphPinOutputSettingsWidget
//------------------------------------------------------------------------------

SLATE_IMPLEMENT_WIDGET(STG_GraphPinOutputSettingsWidget)
void STG_GraphPinOutputSettingsWidget::PrivateRegisterAttributes(FSlateAttributeInitializer& AttributeInitializer)
{
	SLATE_ADD_MEMBER_ATTRIBUTE_DEFINITION_WITH_NAME(AttributeInitializer, "OutputSettings", OutputSettingsAttribute, EInvalidateWidgetReason::Layout)
		.OnValueChanged(FSlateAttributeDescriptor::FAttributeValueChangedDelegate::CreateLambda([](SWidget& Widget)
			{
				//static_cast<STG_GraphPinOutputSettingsWidget&>(Widget).CacheQueryList();
			}));
}

STG_GraphPinOutputSettingsWidget::STG_GraphPinOutputSettingsWidget()
	: OutputSettingsAttribute(*this)
{

}

STG_GraphPinOutputSettingsWidget::~STG_GraphPinOutputSettingsWidget()
{
	/*if (bRegisteredForUndo)
	{
		GEditor->UnregisterForUndo(this);
	}*/
}

void STG_GraphPinOutputSettingsWidget::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	GetPathDelegate.BindRaw(this, &STG_GraphPinOutputSettingsWidget::GetPathAsText);
	PathCommitted.BindRaw(this, &STG_GraphPinOutputSettingsWidget::OnPathCommitted);
	GetNameDelegate.BindRaw(this, &STG_GraphPinOutputSettingsWidget::GetNameAsText);
	NameCommitted.BindRaw(this, &STG_GraphPinOutputSettingsWidget::OnNameCommitted);
	OnGenerateWidthMenu.BindRaw(this, &STG_GraphPinOutputSettingsWidget::OnGenerateWidthEnumMenu);
	GetWidthDelegate.BindRaw(this, &STG_GraphPinOutputSettingsWidget::HandleWidthText);
	OnGenerateHeightMenu.BindRaw(this, &STG_GraphPinOutputSettingsWidget::OnGenerateHeightEnumMenu);
	GetHeightDelegate.BindRaw(this, &STG_GraphPinOutputSettingsWidget::HandleHeightText);
	OnGenerateFormatMenu.BindRaw(this, &STG_GraphPinOutputSettingsWidget::OnGenerateFormatEnumMenu);
	GetFormatDelegate.BindRaw(this, &STG_GraphPinOutputSettingsWidget::HandleFormatText);

	GraphPinObj = InGraphPinObj;

	OnOutputSettingsChanged = InArgs._OnOutputSettingsChanged;

	OutputSettingsAttribute.Assign(*this, InArgs._OutputSettings);

	ChildSlot
	[
		SNew(SVerticalBox)

		//Path Slot
		+ SVerticalBox::Slot()
		.Padding(2)
		.AutoHeight()
		[
			AddEditBoxWithBrowseButton(LOCTEXT("OutputPath", "Path"), GetPathDelegate, PathCommitted)
		]

		//Name Slot
		+ SVerticalBox::Slot()
		//.HAlign(HAlign_Fill)
		.Padding(2)
		.AutoHeight()
		[
			AddEditBox(LOCTEXT("OutputName", "File Name"), GetNameDelegate, NameCommitted)
		]

		//Path Width
		+ SVerticalBox::Slot()
		.Padding(2)
		[
			AddEnumComobox(LOCTEXT("OutputWidth", "Width"), GetWidthDelegate, OnGenerateWidthMenu)
		]

		//Path Height
		+ SVerticalBox::Slot()
		.Padding(2)
		[
			AddEnumComobox(LOCTEXT("OutputWidth", "Height"), GetHeightDelegate, OnGenerateHeightMenu)
		]

		//Path Format
		+ SVerticalBox::Slot()
		.Padding(2)
		[
			AddEnumComobox(LOCTEXT("OutputWidth", "Format"), GetFormatDelegate, OnGenerateFormatMenu)
		]
	];
}

TSharedRef<SWidget> STG_GraphPinOutputSettingsWidget::AddEditBoxWithBrowseButton(FText Label, FGetTextDelegate GetText, FTextCommitted OnTextCommitted)
{
	return SNew(SBox)
		.MinDesiredWidth(150)
		.MaxDesiredWidth(350)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(0.4)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(Label)
				.TextStyle(FAppStyle::Get(), TEXT("Graph.Node.PinName"))
			]

			+ SHorizontalBox::Slot()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Fill)
				.FillWidth(1)
				[
					SNew(SEditableTextBox)
					.Text_Lambda([GetText]() { return GetText.Execute(); })
					.SelectAllTextWhenFocused(true)
					.SelectAllTextOnCommit(true)
					.OnTextCommitted_Lambda([OnTextCommitted](const FText& InText, ETextCommit::Type InCommitType) {
						OnTextCommitted.ExecuteIfBound(InText, InCommitType);
					})
				]

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Fill)
				.AutoWidth()
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.OnClicked(this, &STG_GraphPinOutputSettingsWidget::OnBrowseClick)
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					.ToolTipText(LOCTEXT("Browsepath_ToolTip", "Select the path for the output"))
					[
						SNew(SImage)
						.ColorAndOpacity(FSlateColor::UseForeground())
						.Image(FAppStyle::GetBrush("Icons.BrowseContent"))
					]
				]
			]
		];
}

TSharedRef<SWidget> STG_GraphPinOutputSettingsWidget::AddEditBox(FText Label, FGetTextDelegate GetText, FTextCommitted OnTextCommitted)
{
	return SNew(SBox)
	.MinDesiredWidth(150)
	.MaxDesiredWidth(350)
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.FillWidth(0.4)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(Label)
			.TextStyle(FAppStyle::Get(), TEXT("Graph.Node.PinName"))
		]

		+ SHorizontalBox::Slot()
		[

			SNew(SEditableTextBox)
			.Text_Lambda([GetText]() { return GetText.Execute(); })
			.SelectAllTextWhenFocused(true)
			.SelectAllTextOnCommit(true)
			.OnTextCommitted_Lambda([OnTextCommitted](const FText& InText, ETextCommit::Type InCommitType) {
					OnTextCommitted.ExecuteIfBound(InText, InCommitType);
			})
		]
	];
}

TSharedRef<SWidget> STG_GraphPinOutputSettingsWidget::AddEnumComobox(FText Label, FGetTextDelegate GetText, FGenerateEnumMenu OnGenerateEnumMenu)
{
	return SNew(SBox)
	.MinDesiredWidth(150)
	.MaxDesiredWidth(350)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(0.4)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(Label)
			.TextStyle(FAppStyle::Get(), TEXT("Graph.Node.PinName"))
		]

		+ SHorizontalBox::Slot()
		[
			SNew(SComboButton)
			.OnGetMenuContent(OnGenerateEnumMenu)
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text_Lambda([GetText]() { return GetText.Execute(); })
			]
		]
	];
}

FReply STG_GraphPinOutputSettingsWidget::OnBrowseClick()
{
	FString PackagePath;
	FTG_OutputSettingsCustomization::BrowseFolderPath("", "/Output", PackagePath);
	auto Settings = GetSettings();
	Settings.FolderPath = *PackagePath;
	OnOutputSettingsChanged.ExecuteIfBound(Settings);
	return FReply::Handled();
}

FTG_OutputSettings STG_GraphPinOutputSettingsWidget::GetSettings() const
{
	FString OutputSettingString = GraphPinObj->GetDefaultAsString();
	FTG_OutputSettings Settings;
	Settings.InitFromString(OutputSettingString);

	return Settings;
}

void STG_GraphPinOutputSettingsWidget::GenerateStringsFromEnum(TArray<FString>& OutEnumNames,const FString& EnumPathName)
{
	UEnum* EnumPtr = FindObject<UEnum>(nullptr, *EnumPathName);
	if (EnumPtr)
	{
		for (int32 i = 0; i < EnumPtr->NumEnums() - 1; ++i)
		{
			if (!EnumPtr->HasMetaData(TEXT("Hidden"), i))
			{
				FString DisplayName = EnumPtr->GetDisplayNameTextByIndex(i).ToString();
				uint8 EnumValue = EnumPtr->GetValueByIndex(i);
				UE_LOG(LogTemp, Warning, TEXT("Enum Value: %d, Display Name: %s"), EnumValue, *DisplayName);
				OutEnumNames.Add(DisplayName);
			}
		}
	}
}

int STG_GraphPinOutputSettingsWidget::GetValueFromIndex(const FString& EnumPathName, int Index) const
{
	int Value = 0;
	UEnum* EnumPtr = FindObject<UEnum>(nullptr, *EnumPathName);
	if (EnumPtr)
	{
		Value = EnumPtr->GetValueByIndex(Index);
	}
	return Value;
}

FString STG_GraphPinOutputSettingsWidget::GetEnumValueDisplayName(const FString& EnumPathName, int EnumValue) const
{
	UEnum* EnumPtr = FindObject<UEnum>(nullptr, *EnumPathName, true);
	if (EnumPtr)
	{
		FString DisplayName = EnumPtr->GetDisplayNameTextByValue(EnumValue).ToString();
		return DisplayName;
	}
	return FString();
}

TSharedRef<SWidget> STG_GraphPinOutputSettingsWidget::OnGenerateWidthEnumMenu()
{
	FMenuBuilder MenuBuilder(true, nullptr);
	//Get list for Enum
	TArray<FString> ResolutionEnumItems;
	UEnum* Resolution = StaticEnum<EResolution>();
	GenerateStringsFromEnum(ResolutionEnumItems, Resolution->GetPathName());

	for (int i =0;i< ResolutionEnumItems.Num();i++)
	{
		auto Item = ResolutionEnumItems[i];
		MenuBuilder.AddMenuEntry(
			FText::FromString(Item),
			FText::FromString(Item),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &STG_GraphPinOutputSettingsWidget::HandleWidthChanged, Item , i),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this, i]() {return SelectedWidthIndex == i; })
			));
	}

	return MenuBuilder.MakeWidget();
}

void STG_GraphPinOutputSettingsWidget::HandleWidthChanged(FString Name,int Index)
{
	auto Settings = GetSettings();
	Settings.Width = (EResolution)GetValueFromIndex(StaticEnum<EResolution>()->GetPathName(), Index);
	
	OnOutputSettingsChanged.ExecuteIfBound(Settings);

	SelectedWidthIndex = Index;
	SelectedWidthName = Name;
}

FText STG_GraphPinOutputSettingsWidget::HandleWidthText() const
{
	return FText::FromString(GetEnumValueDisplayName(StaticEnum<EResolution>()->GetPathName(), (int)GetSettings().Width));
}

TSharedRef<SWidget> STG_GraphPinOutputSettingsWidget::OnGenerateHeightEnumMenu()
{
	FMenuBuilder MenuBuilder(true, nullptr);
	//Get list for Enum
	TArray<FString> ResolutionEnumItems;
	GenerateStringsFromEnum(ResolutionEnumItems, StaticEnum<EResolution>()->GetPathName());

	for (int i = 0; i < ResolutionEnumItems.Num(); i++)
	{
		auto Item = ResolutionEnumItems[i];
		MenuBuilder.AddMenuEntry(
			FText::FromString(Item),
			FText::FromString(Item),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &STG_GraphPinOutputSettingsWidget::HandleHeightChanged, Item, i),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this, i]() {return SelectedHeightIndex == i; })
			));
	}

	return MenuBuilder.MakeWidget();
}

void STG_GraphPinOutputSettingsWidget::HandleHeightChanged(FString Name, int Index)
{
	auto Settings = GetSettings();
	Settings.Height = (EResolution)GetValueFromIndex(StaticEnum<EResolution>()->GetPathName(), Index);
	OnOutputSettingsChanged.ExecuteIfBound(Settings);

	SelectedHeightIndex = Index;
}

FText STG_GraphPinOutputSettingsWidget::HandleHeightText() const
{
	return FText::FromString(GetEnumValueDisplayName(StaticEnum<EResolution>()->GetPathName(), (int)GetSettings().Height));
}

TSharedRef<SWidget> STG_GraphPinOutputSettingsWidget::OnGenerateFormatEnumMenu()
{
	FMenuBuilder MenuBuilder(true, nullptr);
	//Get list for Enum
	TArray<FString> ResolutionEnumItems;
	GenerateStringsFromEnum(ResolutionEnumItems, StaticEnum<ETG_TextureFormat>()->GetPathName());

	for (int i = 0; i < ResolutionEnumItems.Num(); i++)
	{
		auto Item = ResolutionEnumItems[i];
		MenuBuilder.AddMenuEntry(
			FText::FromString(Item),
			FText::FromString(Item),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &STG_GraphPinOutputSettingsWidget::HandleFormatChanged, Item, i),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this, i]() {return SelectedFormatIndex == i; })
			));
	}

	return MenuBuilder.MakeWidget();
}

void STG_GraphPinOutputSettingsWidget::HandleFormatChanged(FString Name, int Index)
{
	auto Settings = GetSettings();
	Settings.TextureFormat = (ETG_TextureFormat)GetValueFromIndex(StaticEnum<ETG_TextureFormat>()->GetPathName(), Index);

	OnOutputSettingsChanged.ExecuteIfBound(Settings);

	SelectedFormatIndex = Index;
}

FText STG_GraphPinOutputSettingsWidget::HandleFormatText() const
{
	return FText::FromString(GetEnumValueDisplayName(StaticEnum<ETG_TextureFormat>()->GetPathName(), (int)GetSettings().TextureFormat));
}

FText STG_GraphPinOutputSettingsWidget::GetNameAsText() const
{
	return FText::FromName(GetSettings().BaseName);
}

void STG_GraphPinOutputSettingsWidget::OnNameCommitted(const FText& NewText, ETextCommit::Type /*CommitInfo*/)
{
	auto Settings = GetSettings(); 
	FName BaseName(*NewText.ToString());
	Settings.BaseName = BaseName;

	OnOutputSettingsChanged.ExecuteIfBound(Settings);
}

FText STG_GraphPinOutputSettingsWidget::GetPathAsText() const
{
	return FText::FromName(GetSettings().FolderPath);
}

void STG_GraphPinOutputSettingsWidget::OnPathCommitted(const FText& NewText, ETextCommit::Type /*CommitInfo*/)
{
	FName PathName(*NewText.ToString());
	auto Settings = GetSettings(); 
	Settings.FolderPath = PathName;
	OnOutputSettingsChanged.ExecuteIfBound(Settings);
}

void STG_GraphPinOutputSettingsWidget::PostUndo(bool bSuccess)
{
	
}

void STG_GraphPinOutputSettingsWidget::PostRedo(bool bSuccess)
{

}

#undef LOCTEXT_NAMESPACE
