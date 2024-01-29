// Copyright Epic Games, Inc. All Rights Reserved.

#include "Subsystems/RemoteControlComponentsEditorUtils.h"
#include "Editor/EditorEngine.h"
#include "Framework/Application/SlateApplication.h"
#include "Styles/RemoteControlComponentsEditorStyle.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "RemoteControlComponentsEditorUtils"

FSlateIcon FRemoteControlComponentsEditorUtils::GetIcon(const FName& InIconName)
{
	return FSlateIcon(FRemoteControlComponentsEditorStyle::Get().GetStyleSetName(), InIconName);
}

#undef LOCTEXT_NAMESPACE
