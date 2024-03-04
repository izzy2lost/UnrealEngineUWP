// Copyright Epic Games, Inc. All Rights Reserved.

#if TEXTUREGRAPHEDITOR_ENABLE_NEW_NODE_PREVIEW

#include "STG_NodePreview.h"

#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "SImageViewport.h"
#include "Texture2DPreview.h"
#include "TextureResource.h"
#include "TG_Node.h"
#include "2D/Tex.h"
#include "Device/FX/DeviceBuffer_FX.h"
#include "EdGraph/TG_EdGraphNode.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SCheckBox.h"

#define LOCTEXT_NAMESPACE "STG_NodePreview"

UE::ImageWidgets::IImageViewer::FImageInfo FNodeViewer::GetCurrentImageInfo() const
{
	if (NodeTexture)
	{
		return {{}, {NodeTexture->SizeX, NodeTexture->SizeY}, NodeTexture->GetNumMips(), true};
	}

	return {{}, FIntPoint::ZeroValue, 0, false};
}

void FNodeViewer::DrawCurrentImage(FViewport*, FCanvas* Canvas, const FDrawProperties& Properties)
{
	if (NodeTexture)
	{
		if (FTextureResource* TextureResource = NodeTexture->GetResource(); TextureResource != nullptr)
		{
			TextureResource->bGreyScaleFormat = IsSingleChannel();
			TextureResource->bSRGB = IsSRGB();

			DrawTexture(TextureResource, Canvas, Properties.Placement, Properties.Mip);
		}
	}
}

TOptional<TVariant<FColor, FLinearColor>> FNodeViewer::GetCurrentImagePixelColor(FIntPoint PixelCoords, int32 MipIndex) const
{
	const uint64 PixelIndex = PixelCoords.Y * NodeDescriptor.Width + PixelCoords.X;
	if (PixelIndex < NodePixels.Num())
	{
		const FLinearColor LinearColor = NodePixels[PixelIndex];

		if (NodeDescriptor.Format == BufferFormat::Byte)
		{
			return TVariant<FColor, FLinearColor>(TInPlaceType<FColor>(), LinearColor.ToFColor(IsSRGB()));
		}
		return TVariant<FColor, FLinearColor>(TInPlaceType<FLinearColor>(), LinearColor);
	}
	return {};
}

FText FNodeViewer::GetFormatLabelText() const
{
	if (NodeTexture != nullptr)
	{
		return FText::Format(FTextFormat::FromString("{0}_{1} {2}"),
		                     FText::FromString(TextureHelper::GetChannelsTextFromItemsPerPoint(NodeDescriptor.ItemsPerPoint)),
		                     FText::FromString(BufferDescriptor::FormatToString(NodeDescriptor.Format)),
		                     FText::FromString(NodeDescriptor.bIsSRGB ? "(sRGB)" : "(Linear)"));
	}

	return FText();
}

bool FNodeViewer::IsSingleChannel() const
{
	return NodeDescriptor.ItemsPerPoint == 1;
}

void FNodeViewer::SetTexture(const BlobPtr& InBlob)
{
	NodeTexture = nullptr;

	NodePixels.SetNumUnsafeInternal(0);
	NodeDescriptor = {};

	if (InBlob)
	{
		if (const DeviceBufferPtr Buffer = InBlob->GetBufferRef().GetPtr())
		{
			NodeTexture = GetTextureFromBuffer(Buffer);

			Buffer->Raw()
			      .then([this](const RawBufferPtr& RawBuffer)
			      {
				      RawBuffer->GetAsLinearColor(NodePixels);
				      NodeDescriptor = RawBuffer->GetDescriptor();
			      });
		}
	}
}

void FNodeViewer::SetRGBA(bool bR, bool bG, bool bB, bool bA)
{
	bRGBA[0] = bR;
	bRGBA[1] = bG;
	bRGBA[2] = bB;
	bRGBA[3] = bA;
}

void FNodeViewer::DrawTexture(const FTextureResource* TextureResource, FCanvas* Canvas, const FDrawProperties::FPlacement& Placement,
                              const FDrawProperties::FMip& Mip) const
{
	check(GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5);

	const bool bIsNormalMap = NodeTexture->IsNormalMap();
	const bool bIsVirtualTexture = NodeTexture->IsCurrentlyVirtualTextured();

	const TRefCountPtr BatchedElementParameters = new FBatchedElementTexture2DPreviewParameters(
		Mip.MipLevel, 0, 0, bIsNormalMap, false, false, bIsVirtualTexture, false, Placement.ZoomFactor >= 2.0/*Mip.bUsePointSampling*/);

	FCanvasTileItem Tile(Placement.Offset, TextureResource, Placement.Size, FLinearColor::White);
	Tile.BatchedElementParameters = BatchedElementParameters.GetReference();
	Tile.BlendMode = GetBlendMode();
	Canvas->DrawItem(Tile);
}

ESimpleElementBlendMode FNodeViewer::GetBlendMode() const
{
	if (NodeTexture)
	{
		const TEnumAsByte<TextureCompressionSettings>& CompressionSettings = NodeTexture->CompressionSettings;
		if (CompressionSettings == TC_Grayscale || CompressionSettings == TC_Alpha)
		{
			return SE_BLEND_Opaque;
		}
	}

	int32 Result = SE_BLEND_RGBA_MASK_START;

	if (IsSingleChannel())
	{
		Result += bRGBA[0] * 0b00111;
	}
	else
	{
		Result += bRGBA[0] * 0b00001;
		Result += bRGBA[1] * 0b00010;
		Result += bRGBA[2] * 0b00100;
		Result += bRGBA[3] * 0b01000;
	}

	return static_cast<ESimpleElementBlendMode>(Result);
}

UTextureRenderTarget2D* FNodeViewer::GetTextureFromBuffer(const DeviceBufferPtr& Buffer) const
{
	UTextureRenderTarget2D* OutTexture = nullptr;
	if (const std::shared_ptr<DeviceBuffer_FX> FXBuffer = std::static_pointer_cast<DeviceBuffer_FX>(Buffer))
	{
		if (!FXBuffer->IsNull())
		{
			const TexPtr Tex = FXBuffer->GetTexture();
			check(Tex);
			UTexture* BufferTexture = Tex->GetTexture();
			if (UTextureRenderTarget2D* RenderTarget = Cast<UTextureRenderTarget2D>(BufferTexture))
			{
				OutTexture = RenderTarget;
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("Texture is not a UTextureRenderTarget2D."));
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Buffer is not an FXBuffer."));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("BlobTexture failed to find the buffer."));
	}

	return OutTexture;
}

bool FNodeViewer::IsSRGB() const
{
	return NodeDescriptor.bIsSRGB;
}

class FPreviewViewerCommands : public TCommands<FPreviewViewerCommands>
{
public:
	FPreviewViewerCommands()
		: TCommands(TEXT("NodePreview"), LOCTEXT("ContextDescription", "Node Preview"), NAME_None, FAppStyle::Get().GetStyleSetName())
	{
	}

	virtual void RegisterCommands() override
	{
		UI_COMMAND(ToggleLock, "Toggle Node Preview Lock", "Toggles the node preview lock.", EUserInterfaceActionType::ToggleButton, FInputChord(EKeys::L));
	}

	TSharedPtr<FUICommandInfo> ToggleLock;
};

STG_NodePreviewWidget::~STG_NodePreviewWidget()
{
	FPreviewViewerCommands::Unregister();
}

void STG_NodePreviewWidget::Construct(const FArguments& InArgs)
{
	NodeViewer = MakeShared<FNodeViewer>();

	OnNodeBlobChanged = InArgs._OnNodeBlobChanged;

	FPreviewViewerCommands::Register();
	const FPreviewViewerCommands Commands = FPreviewViewerCommands::Get();
	CommandList = MakeShared<FUICommandList>();
	CommandList->MapAction(Commands.ToggleLock,
	                       FExecuteAction::CreateSP(this, &STG_NodePreviewWidget::ToggleLock),
	                       FCanExecuteAction::CreateLambda([this] { return NodeViewer->GetCurrentImageInfo().bIsValid; }),
	                       FIsActionChecked::CreateLambda([this] { return LockedNode != nullptr; }));

	const TSharedPtr<FExtender> ToolbarExtender = MakeShared<FExtender>();
	ToolbarExtender->AddToolBarExtension("ToolbarLeft", EExtensionHook::Before, CommandList,
	                                     FToolBarExtensionDelegate::CreateSP(this, &STG_NodePreviewWidget::AddLockButton));

	ToolbarExtender->AddToolBarExtension("ToolbarRight", EExtensionHook::After, CommandList,
	                                     FToolBarExtensionDelegate::CreateSP(this, &STG_NodePreviewWidget::AddRGBAButtons));

	const TSharedPtr<UE::ImageWidgets::SImageViewport::FStatusBarExtender> StatusBarExtender = MakeShared<
		UE::ImageWidgets::SImageViewport::FStatusBarExtender>();
	StatusBarExtender->AddExtension("StatusBarLeft", EExtensionHook::After, CommandList,
	                                UE::ImageWidgets::SImageViewport::FStatusBarExtender::FDelegate::CreateSP(this, &STG_NodePreviewWidget::AddFormatLabel));

	ChildSlot
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		[
			SAssignNew(Viewport, UE::ImageWidgets::SImageViewport, NodeViewer.ToSharedRef())
				.ToolbarExtender(ToolbarExtender)
				.StatusBarExtender(StatusBarExtender)
				.DrawSettings(UE::ImageWidgets::SImageViewport::FDrawSettings{
					.ClearColor = FVector3f(0.1f),
					.bBorderEnabled = false,
					.bBackgroundColorEnabled = true,
					.BackgroundColor = FLinearColor::Black,
					.bBackgroundCheckerEnabled = true
				})
				.ControllerSettings(UE::ImageWidgets::SImageViewport::FControllerSettings{
					.DefaultZoomMode = UE::ImageWidgets::SImageViewport::FControllerSettings::EDefaultZoomMode::Fill
				})
		];
}

void STG_NodePreviewWidget::SelectionChanged(UTG_EdGraphNode* Node)
{
	const bool bUpdatePreview = LockedNode == nullptr && SelectedNode != Node;

	SelectedNode = Node;

	if (bUpdatePreview)
	{
		Update();
	}
}

void STG_NodePreviewWidget::NodeDeleted(const UTG_EdGraphNode* Node)
{
	const bool bPreviewNodeDeleted = LockedNode == Node || (LockedNode == nullptr && SelectedNode == Node);

	if (LockedNode == Node)
	{
		LockedNode = nullptr;
	}

	if (SelectedNode == Node)
	{
		SelectedNode = nullptr;
	}

	if (bPreviewNodeDeleted)
	{
		Update();
	}
}

void STG_NodePreviewWidget::Update() const
{
	const UTG_EdGraphNode* PreviewNode = LockedNode ? LockedNode : SelectedNode;

	// Get blob if preview node is valid or assign a nullptr if not.
	const BlobPtr Blob = [PreviewNode]() -> BlobPtr
	{
		if (PreviewNode)
		{
			TArray<FTG_Texture> OutTextures;
			const UTG_Node* TGNode = PreviewNode->GetNode();
			check(TGNode);
			TGNode->GetAllOutputValues(OutTextures);

			if (!OutTextures.IsEmpty())
			{
				return OutTextures[0].RasterBlob;
			}
		}

		return nullptr;
	}();

	// Lambda for actually updating the preview.
	auto UpdatePreview = [this, Blob]
	{
		NodeViewer->SetTexture(Blob);
		Viewport->ResetZoom(NodeViewer->GetCurrentImageInfo().Size);
	};

	if (Blob)
	{
		// Set non-empty preview if blob is valid.
		// Note that if the blob is tiled, it needs to be combined first.
		if (Blob->IsTiled())
		{
			Blob->OnFinalise()
			    .then([Blob]()
			    {
				    const TiledBlobPtr BlobTiled = std::static_pointer_cast<TiledBlob>(Blob);
				    return BlobTiled->CombineTiles(false, false);
			    })
			    .then(UpdatePreview);
		}
		else
		{
			Blob->OnFinalise()
			    .then(UpdatePreview);
		}
	}
	else
	{
		// Set black preview, i.e. Blob is nullptr.
		UpdatePreview();
	}

	// Update preview blob and trigger related external updates.
	[[maybe_unused]] bool Result = OnNodeBlobChanged.ExecuteIfBound(Blob);
}

FReply STG_NodePreviewWidget::OnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (CommandList.IsValid() && CommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void STG_NodePreviewWidget::AddFormatLabel(SHorizontalBox& HorizontalBox)
{
	HorizontalBox.AddSlot()
				 .VAlign(VAlign_Center)
	[
		SNew(STextBlock)
			.Text_Raw(NodeViewer.Get(), &FNodeViewer::GetFormatLabelText)
	];
}

void STG_NodePreviewWidget::AddLockButton(FToolBarBuilder& ToolbarBuilder) const
{
	auto GetLockButtonImage = [this]
	{
		return FSlateIcon(FAppStyle::Get().GetStyleSetName(), LockedNode ? "PropertyWindow.Locked" : "PropertyWindow.Unlocked");
	};

	ToolbarBuilder.AddToolBarButton(FPreviewViewerCommands::Get().ToggleLock, NAME_None, TAttribute<FText>(), TAttribute<FText>(),
	                                TAttribute<FSlateIcon>::CreateLambda(GetLockButtonImage));

	ToolbarBuilder.AddSeparator();
}

void STG_NodePreviewWidget::AddRGBAButtons(FToolBarBuilder& ToolbarBuilder)
{
	ToolbarBuilder.AddSeparator();

	auto GetTextButton = [this](const FString& Label, const FCheckBoxStyle* ButtonStyle, bool& bChecked)
	{
		return SNew(SCheckBox)
			.Style(ButtonStyle)
			.IsEnabled_Lambda([this, &bChecked]
		                      {
			                      return NodeViewer->GetCurrentImageInfo().bIsValid && (&bChecked == &bRGBA[0] ||
				                      !NodeViewer->IsSingleChannel());
		                      })
			.IsChecked(bChecked ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
			.OnCheckStateChanged_Lambda([&bChecked, this](const ECheckBoxState State)
		                      {
			                      bChecked = State == ECheckBoxState::Checked;
			                      NodeViewer->SetRGBA(bRGBA[0], bRGBA[1], bRGBA[2], bRGBA[3]);
		                      })
		[
			SNew(STextBlock)
					.Font(FAppStyle::GetFontStyle("EditorViewportToolBar.Font"))
					.Text(FText::FromString(Label))
		];
	};

	const FCheckBoxStyle* ButtonStyleStart = &FAppStyle::Get().GetWidgetStyle<FCheckBoxStyle>("EditorViewportToolBar.ToggleButton.Start");
	const FCheckBoxStyle* ButtonStyleMiddle = &FAppStyle::Get().GetWidgetStyle<FCheckBoxStyle>("EditorViewportToolBar.ToggleButton.Middle");
	const FCheckBoxStyle* ButtonStyleEnd = &FAppStyle::Get().GetWidgetStyle<FCheckBoxStyle>("EditorViewportToolBar.ToggleButton.End");

	const TSharedRef<SHorizontalBox> RGBA = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			GetTextButton("R", ButtonStyleStart, bRGBA[0])
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			GetTextButton("G", ButtonStyleMiddle, bRGBA[1])
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			GetTextButton("B", ButtonStyleMiddle, bRGBA[2])
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			GetTextButton("A", ButtonStyleEnd, bRGBA[3])
		];

	ToolbarBuilder.AddToolBarWidget(RGBA);
}

void STG_NodePreviewWidget::ToggleLock()
{
	if (LockedNode)
	{
		const bool bUpdatePreview = SelectedNode != LockedNode;

		LockedNode = nullptr;

		if (bUpdatePreview)
		{
			Update();
		}
	}
	else
	{
		LockedNode = SelectedNode;
	}
}

#undef LOCTEXT_NAMESPACE

#endif // TEXTUREGRAPHEDITOR_ENABLE_NEW_NODE_PREVIEW
