// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaType.h"
#include "Containers/ContainersFwd.h"
#include "IAvaEditorExtension.h"
#include "Internationalization/Text.h"
#include "Templates/SharedPointer.h"
#include "Textures/SlateIcon.h"

class FEditorViewportClient;
class SAvaColorPickerPalette;
class SAvaPaletteBase;
class SWidget;
struct FAvaColorChangeData;

DECLARE_DELEGATE_RetVal_OneParam(TSharedRef<SAvaPaletteBase>, FAvaOnPaletteConstruct, const TSharedRef<IToolkitHost>&)

struct FAvaPaletteInfo
{
	FText Title;
	FAvaOnPaletteConstruct ConstructionCallback;
};

struct FAvaPaletteTabInfo
{
	FName Id;
	FText Name;
	FText ToolTip;
	FSlateIcon Icon;
	FAvaOnPaletteConstruct ConstructionCallback;

	FAvaPaletteInfo GetPaletteInfo() const
	{
		return {
			Name,
			ConstructionCallback
		};
	}
};

class FAvaPaletteExtension : public FAvaEditorExtension
{
public:
	UE_AVA_INHERITS_WITH_SUPER(FAvaPaletteExtension, FAvaEditorExtension);

	virtual ~FAvaPaletteExtension() override;

	//~ Begin IAvaEditorExtension
	virtual void Construct(const TSharedRef<IAvaEditor>& InEditor) override;
	virtual void Activate() override;
	virtual void Deactivate() override;
	virtual void RegisterTabSpawners(const TSharedRef<IAvaEditor>& InEditor) const override;
	virtual void ExtendLevelEditorLayout(FLayoutExtender& InExtender) const override;
	virtual void NotifyOnSelectionChanged(const FAvaEditorSelection& InSelection) override;
	//~ End IAvaEditorExtension

	bool RegisterPalette(const FAvaPaletteTabInfo& InPaletteTabInfo);
	TConstArrayView<FAvaPaletteTabInfo> GetRegisteredPalettes() const { return RegisteredPalettes; }

protected:
	TArray<FAvaPaletteTabInfo> RegisteredPalettes;

	void RegisterPalettes();

	void OnColorPicked(const TSharedRef<IToolkitHost>& InEditor, const FAvaColorChangeData& InNewColorData);
};
