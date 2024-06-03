// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewportToolbar/UnrealEdViewportToolbar.h"

#include "EditorViewportClient.h"
#include "SEditorViewport.h"
#include "Templates/SharedPointer.h"

#define LOCTEXT_NAMESPACE "UnrealEdViewportToolbar"

namespace UE::UnrealEd
{

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

} // namespace UE::UnrealEd

#undef LOCTEXT_NAMESPACE
