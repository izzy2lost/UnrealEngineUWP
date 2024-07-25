// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/DisplayClusterViewport_CustomPostProcessSettings.h"
#include "Render/Viewport/DisplayClusterViewport.h"

#include "Render/Viewport/Configuration/DisplayClusterViewportConfigurationHelpers_Postprocess.h"
#include "DisplayClusterConfigurationTypes_Postprocess.h"

int32 GDisplayClusterPostProcessConfigureForViewport = 1;
static FAutoConsoleVariableRef CVarDisplayClusterPostProcessConfigureForViewport(
	TEXT("nDisplay.render.postprocess.ConfigureForViewport"),
	GDisplayClusterPostProcessConfigureForViewport,
	TEXT("Enable changes to some postprocessing parameters depending on the viewport context. (DoF, etc.) (0 to disable).\n"),
	ECVF_RenderThreadSafe
);

///////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterViewport_CustomPostProcessSettings
///////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterViewport_CustomPostProcessSettings::AddCustomPostProcess(const ERenderPass InRenderPass, const FPostProcessSettings& InSettings, float BlendWeight, bool bSingleFrame)
{
	if (BlendWeight > 0.f) // Ignore PP with zero weights
	{
		PostprocessAsset.Emplace(InRenderPass, FPostprocessData(InSettings, BlendWeight, bSingleFrame));
	}
}

void FDisplayClusterViewport_CustomPostProcessSettings::RemoveCustomPostProcess(const ERenderPass InRenderPass)
{
	if (PostprocessAsset.Contains(InRenderPass))
	{
		PostprocessAsset.Remove(InRenderPass);
	}
}

bool FDisplayClusterViewport_CustomPostProcessSettings::GetCustomPostProcess(const ERenderPass InRenderPass, FPostProcessSettings& OutSettings, float* OutBlendWeight) const
{
	const FPostprocessData* ExistSettings = PostprocessAsset.Find(InRenderPass);
	if (ExistSettings && ExistSettings->bIsEnabled)
	{
		OutSettings = ExistSettings->Settings;

		// Returns the weight value, if appropriate.
		if (OutBlendWeight != nullptr)
		{
			*OutBlendWeight = ExistSettings->BlendWeight;
		}

		return true;
	}

	return false;
}

void FDisplayClusterViewport_CustomPostProcessSettings::FinalizeFrame()
{
	// Safe remove items out of iterator
	for (TPair<ERenderPass, FPostprocessData>& It: PostprocessAsset)
	{
		if (It.Value.bIsSingleFrame)
		{
			It.Value.bIsEnabled = false;
		}
	}
}

bool FDisplayClusterViewport_CustomPostProcessSettings::ApplyCustomPostProcess(IDisplayClusterViewport* InViewport, const uint32 InContextNum, const ERenderPass InRenderPass, FPostProcessSettings& InOutPPSettings, float* InOutBlendWeight) const
{
	bool bDidOverride = false;

	switch (InRenderPass)
	{
	case IDisplayClusterViewport_CustomPostProcessSettings::ERenderPass::Start:
	case IDisplayClusterViewport_CustomPostProcessSettings::ERenderPass::Override:
		bDidOverride = GetCustomPostProcess(InRenderPass, InOutPPSettings, InOutBlendWeight);
		break;

	case IDisplayClusterViewport_CustomPostProcessSettings::ERenderPass::Final:
	{
		// Obtaining custom 'Final' PostProcess settings.
		bDidOverride = GetCustomPostProcess(InRenderPass, InOutPPSettings, InOutBlendWeight);

		float PerViewportPPWeight = 0;
		FPostProcessSettings PerViewportPPSettings;

		// The `Final` and `FinalPerViewport` are always applied together.
		// If 'FinalPerViewport' is also used, apply nDisplay ColorGrading as well.
		if (GetCustomPostProcess(IDisplayClusterViewport_CustomPostProcessSettings::ERenderPass::FinalPerViewport, PerViewportPPSettings, &PerViewportPPWeight))
		{
			bDidOverride = true;

			// Extract nDisplay ColorGrading data from PostProcessSettings.
			FDisplayClusterConfigurationViewport_ColorGradingRenderingSettings FinalColorGrading, PerViewportColorGrading;
			FDisplayClusterViewportConfigurationHelpers_Postprocess::CopyPPSStruct(&FinalColorGrading, &InOutPPSettings);
			FDisplayClusterViewportConfigurationHelpers_Postprocess::CopyPPSStructConditional(&PerViewportColorGrading, &PerViewportPPSettings);

			// Blending both using our custom math instead of standard PPS blending
			FDisplayClusterViewportConfigurationHelpers_Postprocess::BlendPostProcessSettings(InOutPPSettings, FinalColorGrading, PerViewportColorGrading);
		}
	}
	break;

	default:
		break;
	}

	// Update post-processing settings for the viewport (DoF, Blur, etc.).
	if (ConfigurePostProcessSettingsForViewport(InViewport, InContextNum, InRenderPass, InOutPPSettings))
	{
		bDidOverride = true;
	}

	return bDidOverride;
}

bool FDisplayClusterViewport_CustomPostProcessSettings::ConfigurePostProcessSettingsForViewport(IDisplayClusterViewport* InViewport, const uint32 InContextNum, const ERenderPass InRenderPass, FPostProcessSettings& InOutPostProcessSettings) const
{
	if (!InViewport || !GDisplayClusterPostProcessConfigureForViewport)
	{
		return false;
	}

	// Todo: Updates DoF and other PP settings for the current viewport.(JIRAs UE-219457,UE-219466)

	return false;
}
