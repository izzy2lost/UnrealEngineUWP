// Copyright Epic Games, Inc. All Rights Reserved.

#include "SourceControlViewportUtils.h"
#include "Settings/EditorStyleSettings.h"
#include "LevelEditorViewport.h"

namespace SourceControlViewportUtils
{

bool GetOverlaySetting(FViewportClient* ViewportClient, ESourceControlStatus Status)
{
	FString CVarName;
	switch (Status)
	{
	case ESourceControlStatus::CheckedOutByOtherUser:
		CVarName = TEXT("SourceControl.Overlays.CheckedOutByOtherUser.Enable");
		break;
	case ESourceControlStatus::NotAtHeadRevision:
		CVarName = TEXT("SourceControl.Overlays.NotAtHeadRevision.Enable");
		break;
	case ESourceControlStatus::CheckedOut:
		CVarName = TEXT("SourceControl.Overlays.CheckedOut.Enable");
		break;
	case ESourceControlStatus::OpenForAdd:
		CVarName = TEXT("SourceControl.Overlays.OpenForAdd.Enable");
		break;
	default:
		checkNoEntry();
		break;
	}

	if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*CVarName); ensure(CVar))
	{
		return CVar->GetBool();
	}

	return false;
}

void SetOverlaySetting(FViewportClient* ViewportClient, ESourceControlStatus Status, bool bEnabled)
{
	FString CVarName;
	switch (Status)
	{
	case ESourceControlStatus::CheckedOutByOtherUser:
		CVarName = TEXT("SourceControl.Overlays.CheckedOutByOtherUser.Enable");
		break;
	case ESourceControlStatus::NotAtHeadRevision:
		CVarName = TEXT("SourceControl.Overlays.NotAtHeadRevision.Enable");
		break;
	case ESourceControlStatus::CheckedOut:
		CVarName = TEXT("SourceControl.Overlays.CheckedOut.Enable");
		break;
	case ESourceControlStatus::OpenForAdd:
		CVarName = TEXT("SourceControl.Overlays.OpenForAdd.Enable");
		break;
	default:
		checkNoEntry();
		break;
	}

	if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*CVarName); ensure(CVar))
	{
		CVar->Set(bEnabled);
	}
}

bool GetFeedbackEnabled(FViewportClient* ViewportClient, ESourceControlStatus Status)
{
	return GetOverlaySetting(ViewportClient, Status);
}

void SetFeedbackEnabled(FViewportClient* ViewportClient, ESourceControlStatus Status, bool bEnabled)
{
	SetOverlaySetting(ViewportClient, Status, bEnabled);

	GEditor->RedrawLevelEditingViewports();
}

uint8 GetFeedbackOpacity(FViewportClient* ViewportClient)
{
	uint8 Opacity = 128;

	if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("SourceControl.Overlays.Alpha")); ensure(CVar))
	{
		Opacity = CVar->GetInt();
	}

	return FMath::Lerp<float>(0.f, 100.f, Opacity / 255.f);
}

void SetFeedbackOpacity(FViewportClient* ViewportClient, uint8 Opacity)
{
	Opacity = FMath::Lerp<float>(0.f, 255.f, Opacity / 100.f);

	if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("SourceControl.Overlays.Alpha")); ensure(CVar))
	{
		CVar->Set(Opacity);
	}

	GEditor->RedrawLevelEditingViewports();
}

}