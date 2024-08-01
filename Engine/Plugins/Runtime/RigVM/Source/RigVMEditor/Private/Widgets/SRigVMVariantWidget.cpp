// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SRigVMVariantWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Images/SImage.h"
#include "DetailLayoutBuilder.h"
#include "Editor/RigVMEditorTools.h"
#include "AssetThumbnail.h"

#define LOCTEXT_NAMESPACE "SRigVMVariantWidget"

SRigVMVariantWidget::SRigVMVariantWidget()
	: VariantRefHash(UINT32_MAX)
{
}

SRigVMVariantWidget::~SRigVMVariantWidget()
{
}

void SRigVMVariantWidget::Construct(
	const FArguments& InArgs)
{
	VariantAttribute = InArgs._Variant;
	
	OnVariantChanged = InArgs._OnVariantChanged;

	VariantRefsAttribute = InArgs._VariantRefs;
	OnCreateVariantRefRow = InArgs._OnCreateVariantRefRow;
	OnBrowseVariantRef = InArgs._OnBrowseVariantRef;

	if(!OnCreateVariantRefRow.IsBound())
	{
		OnCreateVariantRefRow.BindSP(this, &SRigVMVariantWidget::CreateDefaultVariantRefRow);
	}

	ContextAttribute = InArgs._Context;
	if(!ContextAttribute.IsSet() && !ContextAttribute.IsBound())
	{
		ContextAttribute = FRigVMVariantWidgetContext();
	}

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 8, 0, 0)
		.HAlign(HAlign_Left)
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text_Lambda([this]()
			{
				const FRigVMVariant Variant = VariantAttribute.Get();
				return FText::FromString(Variant.Guid.ToString(EGuidFormats::DigitsWithHyphensLower));
			})
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Fill)
		.Padding(0, 8, 0, 0)
		[
			SAssignNew(TagWidget, SRigVMVariantTagWidget)
			.OnGetTags(InArgs._OnGetTags)
			.OnAddTag(InArgs._OnAddTag)
			.OnRemoveTag(InArgs._OnRemoveTag)
			.CanAddTags(InArgs._CanAddTags)
			.EnableContextMenu(InArgs._EnableTagContextMenu)
			.MinDesiredLabelWidth(50.f)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Fill)
		.Padding(0, 8, 0, 0)
		[
			SNew(SScrollBox)
			.Visibility(this, &SRigVMVariantWidget::GetVariantRefListVisibility)
			+ SScrollBox::Slot()
			.MaxSize(InArgs._MaxVariantRefListHeight)
			[
				SAssignNew(VariantRefListBox, SVerticalBox)
			]
		]
	];

	SetCanTick(true);
}

void SRigVMVariantWidget::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SBox::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	uint32 NewHash = 0;
	const TArray<FRigVMVariantRef> NewVariantRefs = VariantRefsAttribute.Get();
	for(const FRigVMVariantRef& NewVariantRef : NewVariantRefs)
	{
		NewHash = HashCombine(NewHash, GetTypeHash(NewVariantRef));
	}

	if(NewHash != VariantRefHash)
	{
		VariantRefHash = NewHash;
		VariantRefs = NewVariantRefs;
		VariantRefs.Sort([](const FRigVMVariantRef& A, const FRigVMVariantRef& B)
		{
			return A.ObjectPath.ToString().Compare(B.ObjectPath.ToString()) < 0;
		});
		
		RebuildVariantRefList();
	}
}

EVisibility SRigVMVariantWidget::GetVariantRefListVisibility() const
{
	return VariantRefs.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
}

TSharedPtr<SWidget> SRigVMVariantWidget::CreateDefaultVariantRefRow(const FRigVMVariantRef& InVariantRef) const
{
	const TAttribute<FText> ToolTipAttribute = FText::FromString(InVariantRef.ObjectPath.ToString());

	const FRigVMVariantRef LocalVariantRef = InVariantRef;
	
	if(!InVariantRef.ObjectPath.IsSubobject())
	{
		const FAssetData AssetData = UE::RigVM::Editor::Tools::FindAssetFromAnyPath(InVariantRef.ObjectPath.ToString(), true);
		const TSharedPtr<FAssetThumbnail> AssetThumbnail = MakeShareable(new FAssetThumbnail(AssetData, 32, 32, TSharedPtr<FAssetThumbnailPool>() ));
		const FAssetThumbnailConfig ThumbnailConfig;

		TSharedRef<SBorder> ThumbnailBorder = SNew(SBorder);
		ThumbnailBorder->SetVisibility(EVisibility::SelfHitTestInvisible);
		ThumbnailBorder->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 4.0f));
		ThumbnailBorder->SetBorderImage(FAppStyle::Get().GetBrush("PropertyEditor.AssetTileItem.DropShadow"));
		ThumbnailBorder->SetContent(
			SNew(SOverlay)
			+SOverlay::Slot()
			.Padding(1.0f)
			[
				SNew(SBorder)
				.Padding(0)
				.BorderImage(FStyleDefaults::GetNoBrush())
				.OnMouseDoubleClick_Lambda(
					[this, LocalVariantRef](
						const FGeometry&,
						const FPointerEvent&) -> FReply
					{
						(void)OnBrowseVariantRef.ExecuteIfBound(LocalVariantRef);
						return FReply::Handled();
					})
				[
					SNew(SBox)
					.ToolTipText(ToolTipAttribute)
					.WidthOverride(32)
					.HeightOverride(32)
					[
						AssetThumbnail->MakeThumbnailWidget(ThumbnailConfig)
					]
				]
			]
			+ SOverlay::Slot()
			[
				SNew(SImage)
				.Image(this, &SRigVMVariantWidget::GetThumbnailBorder, ThumbnailBorder)
				.Visibility(EVisibility::SelfHitTestInvisible)
			]
		);

		return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(0.0f,3.0f,5.0f,0.0f)
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			ThumbnailBorder
		]

		+ SHorizontalBox::Slot()
		.Padding(0.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoHeight()
			[
				SNew(SComboButton)
				.ToolTipText(ToolTipAttribute)
				.IsEnabled(false)
				.ButtonContent()
				[
					SNew(SHorizontalBox)

					+SHorizontalBox::Slot()
					.FillWidth(1)
					.VAlign(VAlign_Center)
					[
						// Show the name of the asset or actor
						SNew(STextBlock)
						.Font( FAppStyle::GetFontStyle( TEXT("PropertyWindow.NormalFont") ) )
						.Text(FText::FromString(InVariantRef.ObjectPath.GetAssetName()))
					]
				]
			]
		];
	}
	
	FString ParentPath = ContextAttribute.Get().ParentPath;

	static const TArray<FString> Separators = { TEXT("\\"), TEXT(":"), TEXT("."), TEXT("/") };

	auto TreatPath = [](FString& InOutPath)
	{
		// replace the first period with an underscore - since that may as well be the object
		// for example: /Game/Animation/ControlRigVariants.ControlRigVariants:FunctionLibrary/MyFunction
		// becomes /Game/Animation/ControlRigVariants_ControlRigVariants/FunctionLibrary/MyFunction
		const int32 FirstPeriodIndex = InOutPath.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromStart);
		if(FirstPeriodIndex != INDEX_NONE)
		{
			InOutPath[FirstPeriodIndex] = TEXT('_');
		}

		// replace all other separators with "/"
		for(const FString& Separator : Separators)
		{
			if(Separator != TEXT("/"))
			{
				InOutPath.ReplaceInline(*Separator, TEXT("/"), ESearchCase::IgnoreCase);
			}
		}
	};

	FString ObjectPath = InVariantRef.ObjectPath.ToString(); 
	FString SearchPath = ObjectPath;

	TreatPath(ParentPath);
	TreatPath(SearchPath);
	SearchPath.ReplaceInline(TEXT("\\"), TEXT("/"), ESearchCase::IgnoreCase);
	SearchPath.ReplaceInline(TEXT(":"), TEXT("/"), ESearchCase::IgnoreCase);
	
	while(!ParentPath.IsEmpty() && !SearchPath.StartsWith(ParentPath, ESearchCase::CaseSensitive))
	{
		if(!ParentPath.Split(TEXT("/"), &ParentPath, nullptr, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
		{
			break;
		}
	}

	if(SearchPath.StartsWith(ParentPath, ESearchCase::CaseSensitive))
	{
		ObjectPath = ObjectPath.Mid(ParentPath.Len());
		for(const FString& Separator : Separators)
		{
			ObjectPath.RemoveFromStart(Separator);
		}
	}

	// cut off double object name
	FString PackageName, ObjectName;
	if(ObjectPath.Split(TEXT("."), &PackageName, &ObjectName, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
	{
		const FString CompleteObjectName = ObjectName + TEXT(".") + ObjectName;
		const FString CompleteObjectNameWithSlash = TEXT("/") + CompleteObjectName;  
		if(ObjectPath.Equals(CompleteObjectName, ESearchCase::CaseSensitive))
		{
			ObjectPath = ObjectName;
		}
		else if(ObjectPath.EndsWith(CompleteObjectNameWithSlash, ESearchCase::CaseSensitive))
		{
			ObjectPath = ObjectPath.LeftChop(CompleteObjectNameWithSlash.Len());
		}
	}

	static constexpr int32 MaxPathLength = 50;
	if(ObjectPath.Len() > MaxPathLength)
	{
		ObjectPath = TEXT("...") + ObjectPath.Right(MaxPathLength - 3);
	}

	TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);

	const FSlateBrush* Icon = nullptr;
	
	static const FString RigVMFunctionLibraryToken = TEXT("/RigVMFunctionLibrary/");
	if(SearchPath.Contains(RigVMFunctionLibraryToken, ESearchCase::CaseSensitive))
	{
		static FSlateIcon FunctionIcon(FAppStyle::GetAppStyleSetName(), "Kismet.AllClasses.FunctionIcon");
		Icon = FunctionIcon.GetIcon(); 
	}

	if(Icon)
	{
		HorizontalBox->AddSlot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(0, 0, 3, 0)
		[
			SNew(SImage)
			.Image(Icon)
			.DesiredSizeOverride(FVector2D(16, 16))
		];
	}

	HorizontalBox->AddSlot()
	.HAlign(HAlign_Left)
	.VAlign(VAlign_Center)
	.AutoWidth()
	.Padding(0, 0, 0, 0)
	[
		SNew(STextBlock)
		.Text(FText::FromString(ObjectPath))
	];

	return SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
		.OnClicked_Lambda([this, LocalVariantRef]() -> FReply
			{
				(void)OnBrowseVariantRef.ExecuteIfBound(LocalVariantRef);
				return FReply::Handled();
			})
		.ContentPadding(FMargin(1, 0))
		.ToolTipText(ToolTipAttribute)
		[
			HorizontalBox
		];
}

void SRigVMVariantWidget::RebuildVariantRefList()
{
	VariantRefListBox->ClearChildren();

	if(VariantRefs.IsEmpty())
	{
		VariantRefListBox->AddSlot()
		.AutoHeight()
		.HAlign(HAlign_Left)
		.Padding(0, 4, 0, 0)
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(LOCTEXT("NoOtherVariants", "No other variants found."))
		];
	}
	else
	{
		VariantRefListBox->AddSlot()
		.AutoHeight()
		.HAlign(HAlign_Left)
		.Padding(0, 4, 0, 0)
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(LOCTEXT("MatchingVariants", "Matching Variants:"))
		];

		for(FRigVMVariantRef VariantRef : VariantRefs)
		{
			TSharedPtr<SWidget> Widget = OnCreateVariantRefRow.Execute(VariantRef);
			Widget->SetOnMouseDoubleClick(
				FPointerEventHandler::CreateLambda(
					[VariantRef, this](const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) -> FReply
					{
						(void)OnBrowseVariantRef.ExecuteIfBound(VariantRef);
						return FReply::Handled();
					}
				)
			);
			VariantRefListBox->AddSlot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			.Padding(0, 4, 0, 0)
			[
				Widget.ToSharedRef()
			];
		}
	}
}

const FSlateBrush* SRigVMVariantWidget::GetThumbnailBorder(TSharedRef<SBorder> InThumbnailBorder) const
{
	static const FName HoveredBorderName("PropertyEditor.AssetThumbnailBorderHovered");
	static const FName RegularBorderName("PropertyEditor.AssetThumbnailBorder");
	return InThumbnailBorder->IsHovered() ? FAppStyle::Get().GetBrush(HoveredBorderName) : FAppStyle::Get().GetBrush(RegularBorderName);
}

#undef LOCTEXT_NAMESPACE // SRigVMVariantWidget
