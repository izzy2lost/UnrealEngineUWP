// Copyright Epic Games, Inc. All Rights Reserved.

#include "Views/Details/Media/DisplayClusterConfiguratorICVFXMediaTileCustomization.h"
#include "Views/Details/Media/DisplayClusterConfiguratorMediaUtils.h"

#include "Components/DisplayClusterICVFXCameraComponent.h"
#include "DisplayClusterConfigurationTypes_Media.h"
#include "DisplayClusterConfiguratorBlueprintEditor.h"
#include "DisplayClusterConfiguratorUtils.h"
#include "DisplayClusterRootActor.h"

#include "IDisplayClusterModularFeatureMediaInitializer.h"

#include "IDetailChildrenBuilder.h"
#include "IPropertyUtilities.h"
#include "PropertyHandle.h"

#include "Features/IModularFeatures.h"

#define LOCTEXT_NAMESPACE "FDisplayClusterConfiguratorICVFXMediaCustomization"


// Enable/disable live auto-configuration. Does not affect "auto-configure" button.
TAutoConsoleVariable<bool> CVarTiledMediaAutoInitializationEnabled(
	TEXT("nDisplay.Media.TiledMediaAutoInitialization"),
	true,
	TEXT("nDisplay tiled media auto-configuration\n")
	TEXT("0 : Disabled\n")
	TEXT("1 : Enabled\n"),
	ECVF_Default
);


//
// Input tiles
//
void FDisplayClusterConfiguratorICVFXMediaInputTileCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InPropertyHandle, IDetailChildrenBuilder& InChildBuilder, IPropertyTypeCustomizationUtils& InCustomizationUtils)
{
	// MediaSource property
	MediaSubjectHandle = GET_CHILD_HANDLE(FDisplayClusterConfigurationMediaUniformTileInput, MediaSource);
	check(MediaSubjectHandle->IsValidHandle());

	// Tile position property
	TilePosHandle = GET_CHILD_HANDLE(FDisplayClusterConfigurationMediaUniformTileInput, Position);
	check(TilePosHandle->IsValidHandle());

	FDisplayClusterConfiguratorICVFXMediaTileCustomizationBase::CustomizeChildren(InPropertyHandle, InChildBuilder, InCustomizationUtils);
}

//
// Output tiles
//
void FDisplayClusterConfiguratorICVFXMediaOutputTileCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InPropertyHandle, IDetailChildrenBuilder& InChildBuilder, IPropertyTypeCustomizationUtils& InCustomizationUtils)
{
	// MediaOutput property
	MediaSubjectHandle = GET_CHILD_HANDLE(FDisplayClusterConfigurationMediaUniformTileOutput, MediaOutput);
	check(MediaSubjectHandle->IsValidHandle());

	// Tile position property
	TilePosHandle = GET_CHILD_HANDLE(FDisplayClusterConfigurationMediaUniformTileOutput, Position);
	check(TilePosHandle->IsValidHandle());

	FDisplayClusterConfiguratorICVFXMediaTileCustomizationBase::CustomizeChildren(InPropertyHandle, InChildBuilder, InCustomizationUtils);
}

//
// Common customization
//

// Static members
bool FDisplayClusterConfiguratorICVFXMediaTileCustomizationBase::bMediaInitializersObtained = false;
TArray<IDisplayClusterModularFeatureMediaInitializer*> FDisplayClusterConfiguratorICVFXMediaTileCustomizationBase::MediaInitializers;

FDisplayClusterConfiguratorICVFXMediaTileCustomizationBase::FDisplayClusterConfiguratorICVFXMediaTileCustomizationBase()
{
	// Obtain media initializer modular features once. We don't expect any of them to be loaded dynamically in runtime.
	if (!bMediaInitializersObtained)
	{
		IModularFeatures& ModularFeatures = IModularFeatures::Get();
		ModularFeatures.LockModularFeatureList();
		MediaInitializers = ModularFeatures.GetModularFeatureImplementations<IDisplayClusterModularFeatureMediaInitializer>(IDisplayClusterModularFeatureMediaInitializer::ModularFeatureName);
		ModularFeatures.UnlockModularFeatureList();

		bMediaInitializersObtained = true;
	}

	// Subscribe for auto-configure event
	FDisplayClusterConfiguratorMediaUtils::Get().OnTiledMediaAutoConfiguration().AddRaw(this, &FDisplayClusterConfiguratorICVFXMediaInputTileCustomization::OnAutoConfigureRequested);
}

FDisplayClusterConfiguratorICVFXMediaTileCustomizationBase::~FDisplayClusterConfiguratorICVFXMediaTileCustomizationBase()
{
	// Unsubscribe from auto-configure event
	FDisplayClusterConfiguratorMediaUtils::Get().OnTiledMediaAutoConfiguration().RemoveAll(this);
}

void FDisplayClusterConfiguratorICVFXMediaTileCustomizationBase::CustomizeChildren(TSharedRef<IPropertyHandle> InPropertyHandle, IDetailChildrenBuilder& InChildBuilder, IPropertyTypeCustomizationUtils& InCustomizationUtils)
{
	if (CVarTiledMediaAutoInitializationEnabled.GetValueOnGameThread())
	{
		// Subscribe for change callbacks
		if (MediaSubjectHandle->IsValidHandle() && TilePosHandle->IsValidHandle())
		{
			MediaSubjectHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateRaw(this, &FDisplayClusterConfiguratorICVFXMediaTileCustomizationBase::OnMediaSubjectChanged));

			TilePosHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateRaw(this, &FDisplayClusterConfiguratorICVFXMediaTileCustomizationBase::OnTilePositionChanged));
			TilePosHandle->SetOnChildPropertyValueChanged(FSimpleDelegate::CreateRaw(this, &FDisplayClusterConfiguratorICVFXMediaTileCustomizationBase::OnTilePositionChanged));
		}
	}

	FDisplayClusterConfiguratorBaseTypeCustomization::CustomizeChildren(InPropertyHandle, InChildBuilder, InCustomizationUtils);
}

void FDisplayClusterConfiguratorICVFXMediaTileCustomizationBase::ModifyMediaSubjectParameters()
{
	if (!EditingObject.IsValid())
	{
		return;
	}

	UDisplayClusterICVFXCameraComponent* ICVFXCameraComponent = Cast<UDisplayClusterICVFXCameraComponent>(EditingObject.Get());
	if (!MediaSubjectHandle->IsValidHandle() || !ICVFXCameraComponent)
	{
		return;
	}

	UObject* NewMediaSubject = nullptr;
	if (MediaSubjectHandle->GetValue(NewMediaSubject) != FPropertyAccess::Success)
	{
		return;
	}

	if (!NewMediaSubject)
	{
		return;
	}

	// Get input tile position
	const FIntPoint TilePos = GetEditedTilePos();
	if (TilePos.X == FIntPoint::NoneValue.X || TilePos.Y == FIntPoint::NoneValue.Y)
	{
		return;
	}

	// Find a suitable initializer, and let it process new media data
	for (IDisplayClusterModularFeatureMediaInitializer* Initializer : MediaInitializers)
	{
		if (Initializer && Initializer->IsMediaSubjectSupported(NewMediaSubject))
		{
			const FString OwnerName = GetOwnerName(ICVFXCameraComponent);
			const uint8 UniqueOwnerIndex = GenerateUniqueOwnerIndex(OwnerName);

			if (UniqueOwnerIndex < TNumericLimits<uint8>::Max())
			{
				Initializer->InitializeMediaSubjectForTile(NewMediaSubject, OwnerName, UniqueOwnerIndex, TilePos);
			}

			break;
		}
	}
}

AActor* FDisplayClusterConfiguratorICVFXMediaTileCustomizationBase::GetOwningActor() const
{
	if (UDisplayClusterICVFXCameraComponent* ICVFXCameraComponent = Cast<UDisplayClusterICVFXCameraComponent>(EditingObject))
	{
		if (AActor* Actor1 = ICVFXCameraComponent->GetOwner())
		{
			return Actor1;
		}
		else if (AActor* Actor2 = Cast<AActor>(FindRootActor()))
		{
			return Actor2;
		}
		else if(FDisplayClusterConfiguratorBlueprintEditor* BlueprintEditor = FDisplayClusterConfiguratorUtils::GetBlueprintEditorForObject(ICVFXCameraComponent))
		{
			return BlueprintEditor->GetPreviewActor();
		}
	}

	return nullptr;
}

FString FDisplayClusterConfiguratorICVFXMediaTileCustomizationBase::GetOwnerName(const UDisplayClusterICVFXCameraComponent* ICVFXCameraComponent) const
{
	// In blueprint editor, the components are transient with the following suffix. We don't need it. The instances in a level don't have such suffix.
	FString CameraName = ICVFXCameraComponent->GetName();
	CameraName.RemoveFromEnd(TEXT("_GEN_VARIABLE"));

	return CameraName;
}

FIntPoint FDisplayClusterConfiguratorICVFXMediaTileCustomizationBase::GetEditedTilePos() const
{
	FIntPoint TilePos = FIntPoint::NoneValue;

	// Children 0 and 1 correspond to X and Y subproperties of FIntPoint
	TilePosHandle->GetChildHandle(0)->GetValue(TilePos.X);
	TilePosHandle->GetChildHandle(1)->GetValue(TilePos.Y);

	return TilePos;
}

uint8 FDisplayClusterConfiguratorICVFXMediaTileCustomizationBase::GenerateUniqueOwnerIndex(const FString& OwnerName) const
{
	if (const AActor* OwningActor = GetOwningActor())
	{
		// Get all camera components
		TArray<UDisplayClusterICVFXCameraComponent*> ICVFXCameras;
		OwningActor->GetComponents(ICVFXCameras);

		if (ICVFXCameras.Num() > 0)
		{
			// Camera sort predicate
			struct FCameraSortPredicate
			{
				bool operator()(UDisplayClusterICVFXCameraComponent& LHS, UDisplayClusterICVFXCameraComponent& RHS) const
				{
					return LHS.GetName().Compare(RHS.GetName(), ESearchCase::IgnoreCase) < 0;
				}
			};

			// Just in case, sort by name to always keep the same order
			ICVFXCameras.Sort(FCameraSortPredicate());

			const int32 CamerasAmount = ICVFXCameras.Num();
			checkSlow(CamerasAmount > 0 && CamerasAmount < TNumericLimits<uint8>::Max());

			// Find camera index in the list
			for (uint8 CameraIdx = 0; CameraIdx < CamerasAmount; ++CameraIdx)
			{
				if (ICVFXCameras[CameraIdx]->GetName().Equals(OwnerName, ESearchCase::IgnoreCase))
				{
					return CameraIdx;
				}
			}
		}
		else
		{
			// Should not normally happen because the camera being edited should be there at least.
			checkSlow(false);
		}
	}

	return TNumericLimits<uint8>::Max();
}

void FDisplayClusterConfiguratorICVFXMediaTileCustomizationBase::OnMediaSubjectChanged()
{
	ModifyMediaSubjectParameters();
}

void FDisplayClusterConfiguratorICVFXMediaTileCustomizationBase::OnTilePositionChanged()
{
	ModifyMediaSubjectParameters();
}

void FDisplayClusterConfiguratorICVFXMediaTileCustomizationBase::OnAutoConfigureRequested(UObject* InEditingObject)
{
	if (EditingObject == InEditingObject)
	{
		ModifyMediaSubjectParameters();
	}
}

#undef LOCTEXT_NAMESPACE
