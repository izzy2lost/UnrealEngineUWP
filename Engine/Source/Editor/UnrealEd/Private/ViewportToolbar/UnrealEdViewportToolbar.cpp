// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewportToolbar/UnrealEdViewportToolbar.h"

#include "DataDrivenShaderPlatformInfo.h"
#include "DebugViewModeHelpers.h"
#include "EditorViewportClient.h"
#include "EditorViewportCommands.h"
#include "GPUSkinCache.h"
#include "GPUSkinCacheVisualizationMenuCommands.h"
#include "RayTracingDebugVisualizationMenuCommands.h"
#include "SEditorViewport.h"
#include "Templates/SharedPointer.h"
#include "ToolMenu.h"
#include "ToolMenuEntry.h"
#include "ToolMenuSection.h"
#include "ViewportToolbar/UnrealEdViewportToolbarContext.h"

#define LOCTEXT_NAMESPACE "UnrealEdViewportToolbar"

namespace UE::UnrealEd::Private
{
int32 CVarToolMenusViewportToolbarsValue = 0;
}

static FAutoConsoleVariableRef CVarToolMenusViewportToolbars(
	TEXT("ToolMenusViewportToolbars"),
	UE::UnrealEd::Private::CVarToolMenusViewportToolbarsValue,
	TEXT("Control whether the new ToolMenus-based viewport toolbars are enabled across the editor. Set to 0 (default) "
		 "to show only the old viewport toolbars. Set to 1 for side-by-side mode where both the old and new viewport "
		 "toolbars are shown. Set to 2 to show only the new viewport toolbars."),
	ECVF_Default
);

namespace UE::UnrealEd
{

bool ShowOldViewportToolbars()
{
	return Private::CVarToolMenusViewportToolbarsValue <= 1;
}

bool ShowNewViewportToolbars()
{
	return Private::CVarToolMenusViewportToolbarsValue >= 1;
}

FText GetViewModesSubmenuLabel(TWeakPtr<SEditorViewport> InViewport)
{
	FText Label = LOCTEXT("ViewMenuTitle_Default", "View");
	if (TSharedPtr<SEditorViewport> PinnedViewport = InViewport.Pin())
	{
		const TSharedPtr<FEditorViewportClient> ViewportClient = PinnedViewport->GetViewportClient();
		check(ViewportClient.IsValid());
		const EViewModeIndex ViewMode = ViewportClient->GetViewMode();
		// If VMI_VisualizeBuffer, return its subcategory name
		if (ViewMode == VMI_VisualizeBuffer)
		{
			Label = ViewportClient->GetCurrentBufferVisualizationModeDisplayName();
		}
		else if (ViewMode == VMI_VisualizeNanite)
		{
			Label = ViewportClient->GetCurrentNaniteVisualizationModeDisplayName();
		}
		else if (ViewMode == VMI_VisualizeLumen)
		{
			Label = ViewportClient->GetCurrentLumenVisualizationModeDisplayName();
		}
		else if (ViewMode == VMI_VisualizeSubstrate)
		{
			Label = ViewportClient->GetCurrentSubstrateVisualizationModeDisplayName();
		}
		else if (ViewMode == VMI_VisualizeGroom)
		{
			Label = ViewportClient->GetCurrentGroomVisualizationModeDisplayName();
		}
		else if (ViewMode == VMI_VisualizeVirtualShadowMap)
		{
			Label = ViewportClient->GetCurrentVirtualShadowMapVisualizationModeDisplayName();
		}
		else if (ViewMode == VMI_VisualizeGPUSkinCache)
		{
			Label = ViewportClient->GetCurrentGPUSkinCacheVisualizationModeDisplayName();
		}
		// For any other category, return its own name
		else
		{
			Label = UViewModeUtils::GetViewModeDisplayName(ViewMode);
		}
	}

	return Label;
}

void PopulateViewModesMenu(
	UToolMenu* InMenu, TSharedRef<SEditorViewport> InViewport, IsViewModeSupportedDelegate InIsViewModeSupported)
{
	const FEditorViewportCommands& BaseViewportActions = FEditorViewportCommands::Get();

	// View modes
	{
		FToolMenuSection& Section = InMenu->AddSection("ViewMode", LOCTEXT("ViewModeHeader", "View Mode"));
		{
			Section.AddMenuEntry(BaseViewportActions.LitMode, UViewModeUtils::GetViewModeDisplayName(VMI_Lit));
			Section.AddMenuEntry(BaseViewportActions.UnlitMode, UViewModeUtils::GetViewModeDisplayName(VMI_Unlit));
			Section.AddMenuEntry(
				BaseViewportActions.WireframeMode, UViewModeUtils::GetViewModeDisplayName(VMI_BrushWireframe));
			Section.AddMenuEntry(
				BaseViewportActions.LitWireframeMode, UViewModeUtils::GetViewModeDisplayName(VMI_Lit_Wireframe));
			Section.AddMenuEntry(
				BaseViewportActions.DetailLightingMode, UViewModeUtils::GetViewModeDisplayName(VMI_Lit_DetailLighting));
			Section.AddMenuEntry(
				BaseViewportActions.LightingOnlyMode, UViewModeUtils::GetViewModeDisplayName(VMI_LightingOnly));
			Section.AddMenuEntry(BaseViewportActions.ReflectionOverrideMode,
				UViewModeUtils::GetViewModeDisplayName(VMI_ReflectionOverride));
			Section.AddMenuEntry(
				BaseViewportActions.CollisionPawn, UViewModeUtils::GetViewModeDisplayName(VMI_CollisionPawn));
			Section.AddMenuEntry(BaseViewportActions.CollisionVisibility,
				UViewModeUtils::GetViewModeDisplayName(VMI_CollisionVisibility));
		}

		if (IsRayTracingEnabled())
		{
			static auto PathTracingCvar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.PathTracing"));
			const bool bPathTracingSupported = FDataDrivenShaderPlatformInfo::GetSupportsPathTracing(GMaxRHIShaderPlatform);
			const bool bPathTracingEnabled = PathTracingCvar && PathTracingCvar->GetValueOnAnyThread() != 0;
			if (bPathTracingSupported && bPathTracingEnabled)
			{
				Section.AddMenuEntry(
					BaseViewportActions.PathTracingMode, UViewModeUtils::GetViewModeDisplayName(VMI_PathTracing));
			}
		}

		// Optimization
		{
			struct Local
			{
				static void BuildOptimizationMenu(UToolMenu* Menu, IsViewModeSupportedDelegate IsViewModeSupported)
				{
					const FEditorViewportCommands& BaseViewportCommands = FEditorViewportCommands::Get();

					UWorld* World = GWorld;
					const ERHIFeatureLevel::Type FeatureLevel = (IsInGameThread() && World)
																  ? (ERHIFeatureLevel::Type)World->GetFeatureLevel()
																  : GMaxRHIFeatureLevel;

					{
						FToolMenuSection& Section = Menu->AddSection(
							"OptimizationViewmodes", LOCTEXT("OptimizationSubMenuHeader", "Optimization Viewmodes"));
						if (FeatureLevel >= ERHIFeatureLevel::SM5)
						{
							Section.AddMenuEntry(BaseViewportCommands.LightComplexityMode,
								UViewModeUtils::GetViewModeDisplayName(VMI_LightComplexity));
							if (IsStaticLightingAllowed())
							{
								Section.AddMenuEntry(BaseViewportCommands.LightmapDensityMode,
									UViewModeUtils::GetViewModeDisplayName(VMI_LightmapDensity));
							}
							Section.AddMenuEntry(BaseViewportCommands.StationaryLightOverlapMode,
								UViewModeUtils::GetViewModeDisplayName(VMI_StationaryLightOverlap));
						}

						Section.AddMenuEntry(BaseViewportCommands.ShaderComplexityMode,
							UViewModeUtils::GetViewModeDisplayName(VMI_ShaderComplexity));

						if (AllowDebugViewShaderMode(
								DVSM_ShaderComplexityContainedQuadOverhead, GMaxRHIShaderPlatform, FeatureLevel))
						{
							Section.AddMenuEntry(BaseViewportCommands.ShaderComplexityWithQuadOverdrawMode,
								UViewModeUtils::GetViewModeDisplayName(VMI_ShaderComplexityWithQuadOverdraw));
						}
						if (AllowDebugViewShaderMode(DVSM_QuadComplexity, GMaxRHIShaderPlatform, FeatureLevel))
						{
							Section.AddMenuEntry(BaseViewportCommands.QuadOverdrawMode,
								UViewModeUtils::GetViewModeDisplayName(VMI_QuadOverdraw));
						}
						if (AllowDebugViewShaderMode(DVSM_LWCComplexity, GMaxRHIShaderPlatform, FeatureLevel))
						{
							Section.AddMenuEntry(BaseViewportCommands.VisualizeLWCComplexity,
								UViewModeUtils::GetViewModeDisplayName(VMI_LWCComplexity), TAttribute<FText>(),
								FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.LWCComplexityMode"));
						}
					}

					{
						FToolMenuSection& Section = Menu->AddSection(
							"TextureStreaming", LOCTEXT("TextureStreamingHeader", "Texture Streaming Accuracy"));

						if (AllowDebugViewShaderMode(DVSM_PrimitiveDistanceAccuracy, GMaxRHIShaderPlatform, FeatureLevel)
							&& (!IsViewModeSupported.IsBound()
								|| IsViewModeSupported.Execute(VMI_PrimitiveDistanceAccuracy)))
						{
							Section.AddMenuEntry(BaseViewportCommands.TexStreamAccPrimitiveDistanceMode,
								UViewModeUtils::GetViewModeDisplayName(VMI_PrimitiveDistanceAccuracy));
						}
						if (AllowDebugViewShaderMode(DVSM_MeshUVDensityAccuracy, GMaxRHIShaderPlatform, FeatureLevel)
							&& (!IsViewModeSupported.IsBound() || IsViewModeSupported.Execute(VMI_MeshUVDensityAccuracy)))
						{
							Section.AddMenuEntry(BaseViewportCommands.TexStreamAccMeshUVDensityMode,
								UViewModeUtils::GetViewModeDisplayName(VMI_MeshUVDensityAccuracy));
						}
						// TexCoordScale accuracy viewmode requires shaders that are only built in the
						// TextureStreamingBuild, which requires the new metrics to be enabled.
						if (AllowDebugViewShaderMode(DVSM_MaterialTextureScaleAccuracy, GMaxRHIShaderPlatform, FeatureLevel)
							&& CVarStreamingUseNewMetrics.GetValueOnAnyThread() != 0
							&& (!IsViewModeSupported.IsBound()
								|| IsViewModeSupported.Execute(VMI_MaterialTextureScaleAccuracy)))
						{
							Section.AddMenuEntry(BaseViewportCommands.TexStreamAccMaterialTextureScaleMode,
								UViewModeUtils::GetViewModeDisplayName(VMI_MaterialTextureScaleAccuracy));
						}
						if (AllowDebugViewShaderMode(DVSM_RequiredTextureResolution, GMaxRHIShaderPlatform, FeatureLevel)
							&& (!IsViewModeSupported.IsBound()
								|| IsViewModeSupported.Execute(VMI_RequiredTextureResolution)))
						{
							Section.AddMenuEntry(BaseViewportCommands.RequiredTextureResolutionMode,
								UViewModeUtils::GetViewModeDisplayName(VMI_RequiredTextureResolution));
						}
						if (AllowDebugViewShaderMode(DVSM_RequiredTextureResolution, GMaxRHIShaderPlatform, FeatureLevel)
							&& (!IsViewModeSupported.IsBound()
								|| IsViewModeSupported.Execute(VMI_VirtualTexturePendingMips)))
						{
							Section.AddMenuEntry(BaseViewportCommands.VirtualTexturePendingMipsMode,
								UViewModeUtils::GetViewModeDisplayName(VMI_VirtualTexturePendingMips));
						}
					}
				}
			};

			Section.AddSubMenu("OptimizationSubMenu", LOCTEXT("OptimizationSubMenu", "Optimization Viewmodes"),
				LOCTEXT("Optimization_ToolTip", "Select optimization visualizer"),
				FNewToolMenuDelegate::CreateStatic(&Local::BuildOptimizationMenu, InIsViewModeSupported),
				FUIAction(FExecuteAction(), FCanExecuteAction(),
					FIsActionChecked::CreateLambda([Viewport = InViewport.ToWeakPtr()]() {
						const TSharedRef<SEditorViewport> ViewportRef = Viewport.Pin().ToSharedRef();
						const TSharedPtr<FEditorViewportClient> ViewportClient = ViewportRef->GetViewportClient();
						check(ViewportClient.IsValid());
						const EViewModeIndex ViewMode = ViewportClient->GetViewMode();
						return (
							// Texture Streaming Accuracy
							ViewMode == VMI_LightComplexity || ViewMode == VMI_LightmapDensity
							|| ViewMode == VMI_StationaryLightOverlap || ViewMode == VMI_ShaderComplexity
							|| ViewMode == VMI_ShaderComplexityWithQuadOverdraw
							|| ViewMode == VMI_QuadOverdraw
							// Texture Streaming Accuracy
							|| ViewMode == VMI_PrimitiveDistanceAccuracy || ViewMode == VMI_MeshUVDensityAccuracy
							|| ViewMode == VMI_MaterialTextureScaleAccuracy || ViewMode == VMI_RequiredTextureResolution
							|| ViewMode == VMI_VirtualTexturePendingMips);
					})),
				EUserInterfaceActionType::RadioButton,
				/* bInOpenSubMenuOnClick = */ false,
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.QuadOverdrawMode"));
		}

		if (IsRayTracingEnabled())
		{
			struct Local
			{
				static void BuildRayTracingDebugMenu(FMenuBuilder& Menu) //, TWeakPtr<SViewportToolBar> InParentToolBar)
				{
					const FRayTracingDebugVisualizationMenuCommands& RtDebugCommands =
						FRayTracingDebugVisualizationMenuCommands::Get();
					RtDebugCommands.BuildVisualisationSubMenu(Menu);
				}
			};

			Section.AddSubMenu("RayTracingDebugSubMenu", LOCTEXT("RayTracingDebugSubMenu", "Ray Tracing Debug"),
				LOCTEXT("RayTracing_ToolTip", "Select ray tracing buffer visualization view modes"),
				FNewMenuDelegate::CreateStatic(&Local::BuildRayTracingDebugMenu), //, ParentToolBar)
				false, FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.RayTracingDebugMode"));
		}

		{
			struct Local
			{
				static void BuildLODMenu(UToolMenu* Menu)
				{
					{
						FToolMenuSection& Section = Menu->AddSection(
							"LevelViewportLODColoration", LOCTEXT("LODModesHeader", "Level of Detail Coloration"));
						Section.AddMenuEntry(FEditorViewportCommands::Get().LODColorationMode,
							UViewModeUtils::GetViewModeDisplayName(VMI_LODColoration));
						Section.AddMenuEntry(FEditorViewportCommands::Get().HLODColorationMode,
							UViewModeUtils::GetViewModeDisplayName(VMI_HLODColoration));
					}
				}
			};

			Section.AddSubMenu("VisualizeGroupedLOD",
				LOCTEXT("VisualizeGroupedLODDisplayName", "Level of Detail Coloration"),
				LOCTEXT("GroupedLODMenu_ToolTip", "Select a mode for LOD Coloration"),
				FNewToolMenuDelegate::CreateStatic(&Local::BuildLODMenu),
				FUIAction(FExecuteAction(), FCanExecuteAction(),
					FIsActionChecked::CreateLambda([WeakViewport = InViewport.ToWeakPtr()]() {
						const TSharedRef<SEditorViewport> ViewportRef = WeakViewport.Pin().ToSharedRef();
						const TSharedPtr<FEditorViewportClient> ViewportClient = ViewportRef->GetViewportClient();
						check(ViewportClient.IsValid());
						const EViewModeIndex ViewMode = ViewportClient->GetViewMode();
						return (ViewMode == VMI_LODColoration || ViewMode == VMI_HLODColoration);
					})),
				EUserInterfaceActionType::RadioButton,
				/* bInOpenSubMenuOnClick = */ false,
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.GroupLODColorationMode"));
		}

		if (GEnableGPUSkinCache)
		{
			Section.AddSubMenu("VisualizeGPUSkinCacheViewMode",
				LOCTEXT("VisualizeGPUSkinCacheViewModeDisplayName", "GPU Skin Cache"),
				LOCTEXT("GPUSkinCacheVisualizationMenu_ToolTip", "Select a mode for GPU Skin Cache visualization."),
				FNewMenuDelegate::CreateStatic(&FGPUSkinCacheVisualizationMenuCommands::BuildVisualisationSubMenu),
				FUIAction(FExecuteAction(), FCanExecuteAction(),
					FIsActionChecked::CreateLambda([WeakViewport = InViewport.ToWeakPtr()]() {
						const TSharedRef<SEditorViewport> ViewportRef = WeakViewport.Pin().ToSharedRef();
						const TSharedPtr<FEditorViewportClient> ViewportClient = ViewportRef->GetViewportClient();
						check(ViewportClient.IsValid());
						return ViewportClient->IsViewModeEnabled(VMI_VisualizeGPUSkinCache);
					})),
				EUserInterfaceActionType::RadioButton,
				/* bInOpenSubMenuOnClick = */ false,
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.VisualizeGPUSkinCacheMode"));
		}
	}

	// Auto Exposure
	{
		const FEditorViewportCommands& BaseViewportCommands = FEditorViewportCommands::Get();

		TSharedRef<SWidget> FixedEV100Menu = InViewport->BuildFixedEV100Menu();
		TSharedPtr<FEditorViewportClient> EditorViewPostClient = InViewport->GetViewportClient();
		const bool bIsLevelEditor = EditorViewPostClient.IsValid() && EditorViewPostClient->IsLevelEditorClient();

		FToolMenuSection& Section = InMenu->AddSection("Exposure", LOCTEXT("ExposureHeader", "Exposure"));
		Section.AddMenuEntry(
			bIsLevelEditor ? BaseViewportCommands.ToggleInGameExposure : BaseViewportCommands.ToggleAutoExposure);
		Section.AddEntry(FToolMenuEntry::InitWidget("FixedEV100", FixedEV100Menu, LOCTEXT("FixedEV100", "EV100")));
	}

	// Wireframe Opacity
	{
		TSharedRef<SWidget> WireOpacityMenu = InViewport->BuildWireframeMenu();
		FToolMenuSection& Section = InMenu->AddSection("Wireframe", LOCTEXT("WireframeHeader", "Wireframe"));
		Section.AddEntry(
			FToolMenuEntry::InitWidget("WireframeOpacity", WireOpacityMenu, LOCTEXT("WireframeOpacity", "Opacity")));
	}
}

FToolMenuEntry CreateViewportToolbarViewModesSubmenu()
{
	// This has to be a dynamic entry for the ViewModes submenu's label to be able to access the context.
	return FToolMenuEntry::InitDynamicEntry(
		"DynamicViewModes",
		FNewToolMenuSectionDelegate::CreateLambda(
			[](FToolMenuSection& InDynamicSection) -> void
			{
				// Base the label on the current view mode.
				TAttribute<FText> LabelAttribute = UE::UnrealEd::GetViewModesSubmenuLabel(nullptr);
				if (UUnrealEdViewportToolbarContext* const Context =
						InDynamicSection.FindContext<UUnrealEdViewportToolbarContext>())
				{
					LabelAttribute = TAttribute<FText>::CreateLambda(
						[WeakViewport = Context->Viewport]()
						{
							return UE::UnrealEd::GetViewModesSubmenuLabel(WeakViewport);
						}
					);
				}

				InDynamicSection.AddSubMenu(
					"ViewModes",
					LabelAttribute,
					LOCTEXT("ViewModesSubmenuTooltip", "View mode settings for the current viewport."),
					FNewToolMenuDelegate::CreateLambda(
						[](UToolMenu* Submenu) -> void
						{
							UUnrealEdViewportToolbarContext* const Context =
								Submenu->FindContext<UUnrealEdViewportToolbarContext>();
							if (!Context)
							{
								return;
							}

							if (const TSharedPtr<SEditorViewport> Viewport = Context->Viewport.Pin())
							{
								PopulateViewModesMenu(Submenu, Viewport.ToSharedRef(), Context->IsViewModeSupported);
							}
						}
					)
				);
			}
		)
	);
}

} // namespace UE::UnrealEd

#undef LOCTEXT_NAMESPACE
