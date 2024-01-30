// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "Widgets/SCompoundWidget.h"

class IAvaEditor;
class IToolkitHost;
struct FAvaPaletteInfo;
struct FAvaPaletteTabInfo;

class SAvaPaletteBase : public SCompoundWidget
{
public:
	SLATE_DECLARE_WIDGET(SAvaPaletteBase, SCompoundWidget)

	SLATE_BEGIN_ARGS(SAvaPaletteBase) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& Args, const TSharedRef<IToolkitHost>& InToolkitHost)
	{
		ToolkitHostWeak = InToolkitHost;
	}

	virtual const FAvaPaletteTabInfo& GetPaletteTabInfo() const = 0;

	TSharedPtr<IToolkitHost> GetToolkitHost() const { return ToolkitHostWeak.Pin(); }

	virtual TSharedPtr<IAvaEditor> GetEditor() const { return EditorWeak.Pin(); }
	virtual void OnCreatedInEditor(TSharedPtr<IAvaEditor> InEditor) { EditorWeak = InEditor; }

protected:
	TWeakPtr<IToolkitHost> ToolkitHostWeak;
	TWeakPtr<IAvaEditor> EditorWeak;
};
