// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Containers/Array.h"
#include "Delegates/IDelegateInstance.h"
#include "Templates/SharedPointer.h"

class FComponentVisualizer;
class FUICommandInfo;
class IAvaDetailsProvider;
class IAvalancheInteractiveToolsModule;
class IDetailsView;
class UEdMode;
struct FAvaInteractiveToolsToolParameters;

class AVALANCHESHAPESEDITOR_API FAvalancheShapesEditorModule : public IModuleInterface
{
public:
	static const FName BevelSprite;
	static const FName BreakSideSprite;
	static const FName ColorSelectionSprite;
	static const FName CornerSprite;
	static const FName DepthSprite;
	static const FName InnerSizeSprite;
	static const FName LinearGradientSprite;
	static const FName NumPointsSprite;
	static const FName NumSidesSprite;
	static const FName TextMaxHeightSprite;
	static const FName TextMaxWidthSprite;
	static const FName TextScaleProportionallySprite;
	static const FName SizeSprite;
	static const FName SlantSprite;
	static const FName UVSprite;

	static FAvalancheShapesEditorModule& Get();

	//~ Begin IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface interface

private:
	TArray<TSharedPtr<FComponentVisualizer>> Visualizers;

	void RegisterShapeTools(IAvalancheInteractiveToolsModule* InModule);
	void RegisterVisualizers();
	
	FDelegateHandle TrackEditorHandle;
};
