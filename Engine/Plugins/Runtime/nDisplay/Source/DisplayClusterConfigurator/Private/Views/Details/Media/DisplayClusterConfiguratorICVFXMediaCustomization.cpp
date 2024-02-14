// Copyright Epic Games, Inc. All Rights Reserved.

#include "Views/Details/Media/DisplayClusterConfiguratorICVFXMediaCustomization.h"

#include "DisplayClusterConfigurationTypes_Media.h"

#include "IDetailChildrenBuilder.h"
#include "IPropertyUtilities.h"
#include "PropertyHandle.h"


#define LOCTEXT_NAMESPACE "FDisplayClusterConfiguratorICVFXMediaCustomization"

void FDisplayClusterConfiguratorICVFXMediaCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InPropertyHandle, IDetailChildrenBuilder& InChildBuilder, IPropertyTypeCustomizationUtils& InCustomizationUtils)
{
	// SplitType property
	TSharedPtr<IPropertyHandle> SplitTypeHandle = GET_CHILD_HANDLE(FDisplayClusterConfigurationMediaICVFX, SplitType);
	check(SplitTypeHandle);

	// Separate groups specific for every split type available
	TArray<TSharedPtr<IPropertyHandle>> FullFramePropertyHandles;
	TArray<TSharedPtr<IPropertyHandle>> UniformTilePropertyHandles;

	// FullFrame properties
	FullFramePropertyHandles.Add(GET_CHILD_HANDLE(FDisplayClusterConfigurationMediaICVFX, MediaInputGroups));
	FullFramePropertyHandles.Add(GET_CHILD_HANDLE(FDisplayClusterConfigurationMediaICVFX, MediaOutputGroups));

	// UniformTile properties
	UniformTilePropertyHandles.Add(GET_CHILD_HANDLE(FDisplayClusterConfigurationMediaICVFX, TiledSplitLayout));
	UniformTilePropertyHandles.Add(GET_CHILD_HANDLE(FDisplayClusterConfigurationMediaICVFX, TileOverscan));
	UniformTilePropertyHandles.Add(GET_CHILD_HANDLE(FDisplayClusterConfigurationMediaICVFX, ClusterNodesToRenderUnboundTiles));
	UniformTilePropertyHandles.Add(GET_CHILD_HANDLE(FDisplayClusterConfigurationMediaICVFX, TiledMediaInputGroups));
	UniformTilePropertyHandles.Add(GET_CHILD_HANDLE(FDisplayClusterConfigurationMediaICVFX, TiledMediaOutputGroups));

	// Get current split type
	EDisplayClusterConfigurationMediaSplitType SplitTypeValue = EDisplayClusterConfigurationMediaSplitType::UniformTiles;
	SplitTypeHandle->GetValue((uint8&)SplitTypeValue);

	TSharedPtr<IPropertyUtilities> PropertyUtils = InCustomizationUtils.GetPropertyUtilities();
	check(PropertyUtils);

	// Setup details update on frustum type change
	SplitTypeHandle->SetOnPropertyValueChanged(
		FSimpleDelegate::CreateLambda([PropertyUtils]()
		{
			if (PropertyUtils)
			{
				PropertyUtils->RequestForceRefresh();
			}
		}));

	// Filter properties to hide
	TArray<TArray<TSharedPtr<IPropertyHandle>>*> HiddenPropertyHandles;
	switch (SplitTypeValue)
	{
	case EDisplayClusterConfigurationMediaSplitType::FullFrame:
		HiddenPropertyHandles.Add(&UniformTilePropertyHandles);
		break;

	case EDisplayClusterConfigurationMediaSplitType::UniformTiles:
		HiddenPropertyHandles.Add(&FullFramePropertyHandles);
		break;

	default:
		checkNoEntry();
	}

	// Hide unnecessary properties depending on the frustum (split) type currently selected
	for (const TArray<TSharedPtr<IPropertyHandle>>* HiddenGroup : HiddenPropertyHandles)
	{
		for (const TSharedPtr<IPropertyHandle>& PropertyHandle : *HiddenGroup)
		{
			PropertyHandle->MarkHiddenByCustomization();
		}
	}

	FDisplayClusterConfiguratorBaseTypeCustomization::CustomizeChildren(InPropertyHandle, InChildBuilder, InCustomizationUtils);
}

#undef LOCTEXT_NAMESPACE
