// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

class FAvaTransitionEditorViewModel;
class IMessageLogListing;
class IMessageToken;
class SWidget;
class UToolMenu;
enum class EStateTreeSaveOnCompile : uint8;
struct FSlateIcon;

class FAvaTransitionCompiler
{
public:
	FAvaTransitionCompiler(FAvaTransitionEditorViewModel& InOwner);

	bool Compile();

	FSlateIcon GetCompileStatusIcon() const;

	TSharedRef<SWidget> GetCompilerResultsWidget() const;

	static void SetSaveOnCompile(EStateTreeSaveOnCompile InSaveOnCompileType);

	static bool HasSaveOnCompile(EStateTreeSaveOnCompile InSaveOnCompileType);

	static void GenerateCompileOptionsMenu(UToolMenu* InMenu);

private:
	void OnMessageTokenClicked(const TSharedRef<IMessageToken>& InMessageToken);

	FAvaTransitionEditorViewModel& Owner;

	TSharedPtr<IMessageLogListing> CompilerResultsListing;

	TSharedPtr<SWidget> CompilerResultsWidget;

	bool bLastCompileSucceeded = true;
};
