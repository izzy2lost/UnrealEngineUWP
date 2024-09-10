// Copyright Epic Games, Inc. All Rights Reserved.
#include "UVEditorUVSnapshotTool.h"

#include "AssetToolsModule.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "ModelingToolTargetUtil.h"
#include "AssetUtils/Texture2DBuilder.h"
#include "Sampling/MeshUVShellMapEvaluator.h"
#include "ModelingObjectsCreationAPI.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TargetInterfaces/StaticMeshBackedTarget.h"
#include "UVEditorUXSettings.h"
#include "Drawing/MeshElementsVisualizer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UVEditorUVSnapshotTool)

using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "UUVEditorUVSnapshotTool"

/*
 * ToolBuilder
 */
bool UUVEditorUVSnapshotToolBuilder::CanBuildTool(const FToolBuilderState& SceneState) const
{
	return Targets && Targets->Num() == 1;
}
UInteractiveTool* UUVEditorUVSnapshotToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	UUVEditorUVSnapshotTool* NewTool = NewObject<UUVEditorUVSnapshotTool>(SceneState.ToolManager);
	NewTool->SetTarget((*Targets)[0]);
	return NewTool;
}

/*
 * Operator
 */
class FMeshUVMapBakerOp : public TGenericDataOperator<FMeshMapBaker>
{
public:
	using ImagePtr = TSharedPtr<UE::Geometry::TImageBuilder<FVector4f>, ESPMode::ThreadSafe>;
	// General bake settings
	TSharedPtr<UE::Geometry::FDynamicMesh3, ESPMode::ThreadSafe> BaseMesh;
	TUniquePtr<UE::Geometry::FMeshMapBaker> Baker;
	UUVEditorUVSnapshotTool::FUVSnapshotBakeSettings BakeSettings;
	TSharedPtr<UE::Geometry::FDynamicMeshAABBTree3, ESPMode::ThreadSafe> DetailSpatial;

	virtual void CalculateResult(FProgressCancel* Progress) override
	{
		Baker = MakeUnique<FMeshMapBaker>();
		Baker->CancelF = [Progress]() {
			return Progress && Progress->Cancelled();
		};
		Baker->SetTargetMesh(BaseMesh.Get());
		Baker->SetTargetMeshUVLayer(BakeSettings.UVLayer);
		Baker->SetDimensions(BakeSettings.Dimensions);
		Baker->SetSamplesPerPixel(BakeSettings.SamplesPerPixel);
		
		FMeshBakerDynamicMeshSampler DetailSampler(BaseMesh.Get(), DetailSpatial.Get(), nullptr);
		Baker->SetDetailSampler(&DetailSampler);
		const TSharedPtr<FMeshUVShellMapEvaluator> UVShellEval = MakeShared<FMeshUVShellMapEvaluator>();
		UVShellEval->TexelSize = BakeSettings.Dimensions.GetTexelSize();
		UVShellEval->UVLayer = BakeSettings.UVLayer;
		UVShellEval->WireframeThickness = BakeSettings.WireframeThickness;
		UVShellEval->WireframeColor = BakeSettings.WireframeColor;
		UVShellEval->ShellColor = BakeSettings.ShellColor;
		UVShellEval->BackgroundColor = BakeSettings.BackgroundColor;
		Baker->AddEvaluator(UVShellEval);
		Baker->Bake();
		SetResult(MoveTemp(Baker));
	}
};

/*
 * Tool
 */
void UUVEditorUVSnapshotTool::Setup()
{
	UInteractiveTool::Setup();

	// initialize properties
	UVShellSettings = NewObject<UUVEditorBakeUVShellProperties>(this);
	UVShellSettings->RestoreProperties(this);
	AddToolPropertySource(UVShellSettings);

	// retrieve whatever UV Layer is currently being displayed in UV Editor
	UVShellSettings->UVLayer = Target->UVLayerIndex;
	
	UVShellSettings->WatchProperty(UVShellSettings->SamplesPerPixel, [this](EBakeTextureSamplesPerPixel) { OpState |= EUVSnapshotBakeOpState::Evaluate; });
	UVShellSettings->WatchProperty(UVShellSettings->Resolution, [this](EBakeTextureResolution) { OpState |= EUVSnapshotBakeOpState::Evaluate; });
	UVShellSettings->WatchProperty(UVShellSettings->UVLayer, [this](int) { OpState |= EUVSnapshotBakeOpState::Evaluate; });
	UVShellSettings->WatchProperty(UVShellSettings->WireframeThickness, [this](float) { OpState |= EUVSnapshotBakeOpState::Evaluate; });
	UVShellSettings->WatchProperty(UVShellSettings->WireframeColor, [this](FLinearColor) { OpState |= EUVSnapshotBakeOpState::Evaluate; });
	UVShellSettings->WatchProperty(UVShellSettings->ShellColor, [this](FLinearColor) { OpState |= EUVSnapshotBakeOpState::Evaluate; });
	UVShellSettings->WatchProperty(UVShellSettings->BackgroundColor, [this](FLinearColor) { OpState |= EUVSnapshotBakeOpState::Evaluate; });
	SetToolPropertySourceEnabled(UVShellSettings, true);
	
	ResultSettings = NewObject<UUVEditorBakeUVShellResultProperties>(this);
	ResultSettings->RestoreProperties(this);
	AddToolPropertySource(ResultSettings);
	SetToolPropertySourceEnabled(ResultSettings, true);
	ResultSettings->Result = nullptr;

	// set up the detail mesh & spatial
	FDynamicMesh3 DetailMeshGet;
	Target->AppliedPreview->GetCurrentResultCopy(DetailMeshGet, false);
	DetailMesh = MakeShared<FDynamicMesh3, ESPMode::ThreadSafe>(DetailMeshGet);
	
	DetailSpatial = MakeShared<FDynamicMeshAABBTree3, ESPMode::ThreadSafe>();
	DetailSpatial->SetMesh(DetailMesh.Get(), true);

	// mark for evaluation
	OpState |= EUVSnapshotBakeOpState::Evaluate;

	CachedBakeSettings = FUVSnapshotBakeSettings();

	// set up UPreviewGeometry for visualization in Unwrap viewport
	PreviewGeoBackgroundQuad = NewObject<UPreviewGeometry>(this);
	PreviewGeoBackgroundQuad->CreateInWorld(Target->UnwrapPreview->GetWorld(), FTransform::Identity);
	PreviewGeoBackgroundQuad->AddTriangleSet("UVShellMap");
	PreviewGeoBackgroundQuad->SetAllVisible(true);

	// set up op factory
	SetToolDisplayName(LOCTEXT("ToolNameLocal", "UV Snapshot"));
	GetToolManager()->DisplayMessage(
		LOCTEXT("OnStartUVSnapshotTool", "Export a texture asset of a UV Layout."),
		EToolMessageLevel::UserNotification);
	
}
void UUVEditorUVSnapshotTool::Shutdown(EToolShutdownType ShutdownType)
{
	if (ShutdownType == EToolShutdownType::Accept)
	{
		const IStaticMeshBackedTarget* StaticMeshTarget = Cast<IStaticMeshBackedTarget>(Target->SourceTarget);
		UObject* SourceAsset = StaticMeshTarget ? StaticMeshTarget->GetStaticMesh() : nullptr;
		const UPrimitiveComponent* SourceComponent = UE::ToolTarget::GetTargetComponent(Target->SourceTarget);
		CreateTextureAsset(ResultSettings->Result, SourceComponent->GetWorld(), SourceAsset);
	}
	
	UVShellSettings->SaveProperties(this);
	if (UVShellSettings)
	{
		UVShellSettings->RestoreProperties(this, TEXT("UVEditorUVSnapshotTool"));
		UVShellSettings = nullptr;
	}
	if (Compute)
	{
		Compute->Shutdown();
	}

	Target->AppliedPreview->ClearOpFactory();

	if (PreviewGeoBackgroundQuad)
	{
		PreviewGeoBackgroundQuad->Disconnect();
		PreviewGeoBackgroundQuad = nullptr;
	}
	// re-enable wireframe display and unwrap preview
	Target->WireframeDisplay->Settings->bVisible = true;
	Target->UnwrapPreview->SetVisibility(true);
}
void UUVEditorUVSnapshotTool::OnTick(float DeltaTime)
{
	if (Compute)
	{
		Compute->Tick(DeltaTime);
	}
}
void UUVEditorUVSnapshotTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	UpdateResult();
}
TUniquePtr<UE::Geometry::TGenericDataOperator<UE::Geometry::FMeshMapBaker>> UUVEditorUVSnapshotTool::MakeNewOperator()
{
	TUniquePtr<FMeshUVMapBakerOp> Op = MakeUnique<FMeshUVMapBakerOp>();
	Op->DetailSpatial = DetailSpatial;
	Op->BaseMesh = DetailMesh;
	Op->BakeSettings = CachedBakeSettings;
	
	return Op;
}
void UUVEditorUVSnapshotTool::UpdateResult()
{
	if (OpState == EUVSnapshotBakeOpState::Clean)
	{
		return;
	}
	// clear warning (ugh)
	GetToolManager()->DisplayMessage(FText(), EToolMessageLevel::UserWarning);

	const int32 ImageSize = (int32)UVShellSettings->Resolution;
	const FImageDimensions Dimensions(ImageSize, ImageSize);
	CachedBakeSettings.Dimensions = Dimensions;
	
	// Clear our invalid bitflag to check again for valid inputs.
	OpState &= ~EUVSnapshotBakeOpState::Invalid;
	OpState |= UpdateResult_UVShellMap();
	
	// Early exit if op input parameters are invalid.
	if ((bool)(OpState & EUVSnapshotBakeOpState::Invalid))
	{
		InvalidateResults();
		return;
	}
	// This should be the only point of compute invalidation to
	// minimize synchronization issues.
	InvalidateCompute();
}
void UUVEditorUVSnapshotTool::OnMapUpdated(const TUniquePtr<UE::Geometry::FMeshMapBaker>& NewResult)
{
	const FImageDimensions BakeDimensions = NewResult->GetDimensions();
	constexpr bool bConvertToSRGB = true;
	FTexture2DBuilder TextureBuilder;
	TextureBuilder.Initialize(FTexture2DBuilder::ETextureType::Color, BakeDimensions);
	TextureBuilder.Copy(*NewResult->GetBakeResults(0)[0], bConvertToSRGB);
	TextureBuilder.Commit(false);
	// Copy image to source data after commit. This will avoid incurring
	// the cost of hitting the DDC for texture compile while iterating on
	// bake settings. Since this dirties the texture, the next time the texture
	// is used after accepting the final texture, the DDC will trigger and
	// properly recompile the platform data.
	constexpr bool bConvertSourceToSRGB = true;
	// default should be ChannelBits8
	constexpr ETextureSourceFormat SourceDataFormat = TSF_BGRA8;
	TextureBuilder.CopyImageToSourceData(*NewResult->GetBakeResults(0)[0], SourceDataFormat, bConvertSourceToSRGB);
	// The CachedUVMap can be thrown out of sync if updated during a background
	// compute. Validate the computed type against our cached map.
	CachedUVMap = TextureBuilder.GetTexture2D();
	UpdateVisualization();
	GetToolManager()->PostInvalidation();
	
}
void UUVEditorUVSnapshotTool::InvalidateResults() const
{
	ResultSettings->Result = nullptr;
}
void UUVEditorUVSnapshotTool::InvalidateCompute()
{
	if (!Compute)
	{
		// Initialize background compute
		Compute = MakeUnique<TGenericDataBackgroundCompute<FMeshMapBaker>>();
		Compute->Setup(this);
		Compute->OnResultUpdated.AddLambda([this](const TUniquePtr<FMeshMapBaker>& NewResult) { OnMapUpdated(NewResult); });
	}
	Compute->InvalidateResult();
	OpState = EUVSnapshotBakeOpState::Clean;
}
void UUVEditorUVSnapshotTool::UpdateVisualization() 
{
	ResultSettings->Result = CachedUVMap;
	UpdatePreviewMaterialBasedOnBackground();
}
EUVSnapshotBakeOpState UUVEditorUVSnapshotTool::UpdateResult_UVShellMap()
{
	EUVSnapshotBakeOpState ResultState = EUVSnapshotBakeOpState::Clean;
	FUVSnapshotBakeSettings UVShellMapSettings;
	UVShellMapSettings.UVLayer = UVShellSettings->UVLayer;
	UVShellMapSettings.WireframeThickness = UVShellSettings->WireframeThickness;
	UVShellMapSettings.WireframeColor = UVShellSettings->WireframeColor;
	UVShellMapSettings.ShellColor = UVShellSettings->ShellColor;
	UVShellMapSettings.BackgroundColor = UVShellSettings->BackgroundColor;

	const int32 ImageSize = (int32)UVShellSettings->Resolution;
	const FImageDimensions Dimensions(ImageSize, ImageSize);
	UVShellMapSettings.Dimensions = Dimensions;
	UVShellMapSettings.SamplesPerPixel = (int32)UVShellSettings->SamplesPerPixel;

	// if our settings have changed
	if (CachedBakeSettings != UVShellMapSettings)
	{
		CachedBakeSettings = UVShellMapSettings;
		ResultState |= EUVSnapshotBakeOpState::Evaluate;
	}
	return ResultState;
}
void UUVEditorUVSnapshotTool::UpdatePreviewMaterialBasedOnBackground()
{
	// using this material so that we can set a texture
	UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(LoadObject<UMaterial>(nullptr, TEXT("/UVEditor/Materials/UVEditorBackground")), this);
	Mat->SetTextureParameterValue(TEXT("BackgroundBaseMap_Color"), ResultSettings->Result);
	Mat->SetScalarParameterValue(TEXT("BackgroundPixelDepthOffset"), FUVEditorUXSettings::BackgroundQuadDepthOffset - 0.5);	

	// temporarily disable wireframe overlay and unwrap preview; re-enabled on shutdown
	Target->WireframeDisplay->Settings->bVisible = false;
	Target->UnwrapPreview->SetVisibility(false);

	// connect to existing Preview Geometry
	UTriangleSetComponent* TriangleSet = PreviewGeoBackgroundQuad->FindTriangleSet("UVShellMap");
	TriangleSet->Clear();

	const FVector Normal(0, 0, 1);
	const FColor BackgroundColor = FColor::Red;

	// set up rendering of 2 triangles to make one 2d quad
	FIndex2i UDimBlockToRender = FIndex2i(0,0);

	auto MakeQuadVert = [&UDimBlockToRender, &Normal, &BackgroundColor](int32 CornerX, int32 CornerY)
	{
		FVector2f ExternalUV(static_cast<float>(UDimBlockToRender.A + CornerX), static_cast<float>(UDimBlockToRender.B + CornerY));

		return FRenderableTriangleVertex(FUVEditorUXSettings::ExternalUVToUnwrapWorldPosition(ExternalUV),
			(FVector2D)FUVEditorUXSettings::ExternalUVToInternalUV(ExternalUV),
			Normal, BackgroundColor);
	};
	
	FRenderableTriangleVertex V00 = MakeQuadVert(0,0);
	FRenderableTriangleVertex V10 = MakeQuadVert(1, 0);
	FRenderableTriangleVertex V11 = MakeQuadVert(1, 1);
	FRenderableTriangleVertex V01 = MakeQuadVert(0,1);

	// add to TriangleSet in PreviewGeometry
	TriangleSet->AddTriangle(FRenderableTriangle(Mat, V00, V10, V11));
	TriangleSet->AddTriangle(FRenderableTriangle(Mat, V00, V11, V01));
	
}
void UUVEditorUVSnapshotTool::CreateTextureAsset(const TObjectPtr<UTexture2D>& Texture, UWorld* SourceWorld, UObject* SourceAsset) const
{
	bool bCreatedAssetOK = true;
	const FString ObjName = UE::ToolTarget::GetTargetActor(Target->SourceTarget)->GetActorNameOrLabel();
	FString NewAssetName = FString::Printf(TEXT("%s_UVShell_UV%d"), *ObjName, UVShellSettings->UVLayer); // will be something like "Cylinder_UVShell_UV0"

	// open dialog so user can choose where to save out the new asset
	IContentBrowserSingleton& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();

	FSaveAssetDialogConfig Config;
	Config.DefaultAssetName = NewAssetName;
	Config.DialogTitleOverride = LOCTEXT("GenerateStaticMeshActorPathDialogWarning", "Choose Folder Path and Name for New Asset. Cancel to Discard New Asset.");
	// if we have previously saved a UVSnapshot, use that path as default; otherwise default path
	Config.DefaultPath = UVShellSettings->SavedPath.IsEmpty() ?  FString() : UVShellSettings->SavedPath;
	
	const FString SelectedPath = ContentBrowser.CreateModalSaveAssetDialog(Config);

	// ensures that if save dialog is closed without saving, nothing happens
	if (SelectedPath.IsEmpty() == false)
	{
		// save path so that if UV Snapshot is performed again, when save dialog opens we are already in previous location
		UVShellSettings->SavedPath = FPaths::GetPath(SelectedPath);
		NewAssetName = FPaths::GetBaseFilename(SelectedPath, true);

		FString PackageNameOut, AssetNameOut;
		const FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
		AssetToolsModule.Get().CreateUniqueAssetName(
			FPaths::Combine(UVShellSettings->SavedPath, NewAssetName), TEXT(""),
			PackageNameOut, AssetNameOut);

		// create the asset
		bCreatedAssetOK = bCreatedAssetOK && UE::Modeling::CreateTextureObject( GetToolManager(),
			FCreateTextureObjectParams{ 0, SourceWorld, SourceAsset, NewAssetName, Texture}).IsOK();
	}
		
	ensure(bCreatedAssetOK);
}
#undef LOCTEXT_NAMESPACE