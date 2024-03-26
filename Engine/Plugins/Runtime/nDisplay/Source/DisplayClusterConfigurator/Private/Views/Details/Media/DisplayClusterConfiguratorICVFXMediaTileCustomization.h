// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Views/Details/DisplayClusterConfiguratorBaseTypeCustomization.h"

class IDisplayClusterModularFeatureMediaInitializer;
class UDisplayClusterICVFXCameraComponent;


/**
 * Helper class to hanldle common functionality of input and output tiles customization
 */
class FDisplayClusterConfiguratorICVFXMediaTileCustomizationBase
	: public FDisplayClusterConfiguratorBaseTypeCustomization
{
public:
	FDisplayClusterConfiguratorICVFXMediaTileCustomizationBase();
	~FDisplayClusterConfiguratorICVFXMediaTileCustomizationBase();

protected:

	//~ Begin IPropertyTypeCustomization
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	//~ End IPropertyTypeCustomization

private:

	/** Entry point to modify media subjects. */
	void ModifyMediaSubjectParameters();

	/** Returns the actor that owns the object being edited. */
	AActor* GetOwningActor() const;

	/** Returns the name of media subject's owner. */
	FString GetOwnerName(const UDisplayClusterICVFXCameraComponent* ICVFXCameraComponent) const;

	/** Returns tile position currently set. */
	FIntPoint GetEditedTilePos() const;

	/** Generate unique index for the media subject's owner. */
	uint8 GenerateUniqueOwnerIndex(const FString& OwnerName) const;

private:

	/** Handles media source/output change callbacks */
	void OnMediaSubjectChanged();

	/** Handles tile position change callbacks */
	void OnTilePositionChanged();

	/** Auto-configuration requests handler */
	void OnAutoConfigureRequested(UObject* EditingObject);

protected:

	/** MediaSource or MediaOutput property handle, depending on the child implementation. */
	TSharedPtr<IPropertyHandle> MediaSubjectHandle;

	/** Tile position property handle. */
	TSharedPtr<IPropertyHandle> TilePosHandle;

private:

	/** Media initializers (modular features) available. */
	static TArray<IDisplayClusterModularFeatureMediaInitializer*> MediaInitializers;

	/** Optimization flag to prevent retrieving modular features multiple times.*/
	static bool bMediaInitializersObtained;
};


/**
 * Details panel customization for FDisplayClusterConfigurationMediaUniformTileInput struct (input tiles).
 */
class FDisplayClusterConfiguratorICVFXMediaInputTileCustomization
	: public FDisplayClusterConfiguratorICVFXMediaTileCustomizationBase
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShared<FDisplayClusterConfiguratorICVFXMediaInputTileCustomization>();
	}

protected:
	//~ Begin IPropertyTypeCustomization
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	//~ End IPropertyTypeCustomization
};


/**
 * Details panel customization for FDisplayClusterConfigurationMediaUniformTileOutput struct (output tiles).
 */
class FDisplayClusterConfiguratorICVFXMediaOutputTileCustomization
	: public FDisplayClusterConfiguratorICVFXMediaTileCustomizationBase
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShared<FDisplayClusterConfiguratorICVFXMediaOutputTileCustomization>();
	}

protected:
	//~ Begin IPropertyTypeCustomization
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	//~ End IPropertyTypeCustomization
};
