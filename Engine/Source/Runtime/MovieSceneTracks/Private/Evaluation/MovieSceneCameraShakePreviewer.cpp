// Copyright Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneCameraShakePreviewer.h"

#if WITH_EDITOR

#include "LevelEditorViewport.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/CameraShakeSourceComponent.h"
#include "Camera/CameraModifier_CameraShake.h"

FCameraShakePreviewer::FCameraShakePreviewer()
	: LastDeltaTime(0.f)
	, LastLocationModifier(FVector::ZeroVector)
	, LastRotationModifier(FRotator::ZeroRotator)
	, LastFOVModifier(0.f)
{
}

FCameraShakePreviewer::~FCameraShakePreviewer()
{
	if (!ensureMsgf(RegisteredViewportClients.Num() == 0, TEXT("Forgot to call UnRegisterViewModifier!")))
	{
		UnRegisterViewModifier();
	}
}

UCameraShakeBase* FCameraShakePreviewer::AddCameraShake(const FCameraShakePreviewerAddParams& Params)
{
	UCameraShakeBase* NewShake = NewObject<UCameraShakeBase>(GetTransientPackage(), Params.ShakeClass);
	ActiveShakes.Add({ NewShake, Params.SourceComponent, Params.GlobalStartTime });

	FCameraShakeBaseStartParams StartParams;
	StartParams.Scale = Params.Scale;
	StartParams.PlaySpace = Params.PlaySpace;
	StartParams.UserPlaySpaceRot = Params.UserPlaySpaceRot;
	StartParams.DurationOverride = Params.DurationOverride;
	NewShake->StartShake(StartParams);

	return NewShake;
}

void FCameraShakePreviewer::RemoveCameraShake(UCameraShakeBase* ShakeInstance)
{
	const bool bImmediately = true;
	for (int32 i = ActiveShakes.Num() - 1; i >= 0; --i)
	{
		FPreviewCameraShakeInfo& ActiveShake = ActiveShakes[i];
		if (ActiveShake.ShakeInstance == ShakeInstance)
		{
			ActiveShake.ShakeInstance->StopShake(bImmediately);
			ActiveShake.ShakeInstance->TeardownShake();
			ActiveShakes.RemoveAt(i, 1);
			break;
		}
	}
}

void FCameraShakePreviewer::RemoveAllCameraShakesFromSource(const UCameraShakeSourceComponent* SourceComponent)
{
	const bool bImmediately = true;
	for (int32 i = ActiveShakes.Num() - 1; i >= 0; --i)
	{
		FPreviewCameraShakeInfo& ActiveShake = ActiveShakes[i];
		if (ActiveShake.SourceComponent.Get() == SourceComponent && ActiveShake.ShakeInstance != nullptr)
		{
			ActiveShake.ShakeInstance->StopShake(bImmediately);
			ActiveShake.ShakeInstance->TeardownShake();
			ActiveShakes.RemoveAt(i, 1);
		}
	}
}

void FCameraShakePreviewer::RemoveAllCameraShakes()
{
	const bool bImmediately = true;
	for (FPreviewCameraShakeInfo& ActiveShake : ActiveShakes)
	{
		if (ActiveShake.ShakeInstance)
		{
			ActiveShake.ShakeInstance->StopShake(bImmediately);
			ActiveShake.ShakeInstance->TeardownShake();
		}
	}
	ActiveShakes.Empty();
}

void FCameraShakePreviewer::GetActiveCameraShakes(TArray<FActiveCameraShakeInfo>& ActiveCameraShakes) const
{
	for (const FPreviewCameraShakeInfo& ActiveShake : ActiveShakes)
	{
		FActiveCameraShakeInfo ShakeInfo;
		ShakeInfo.ShakeInstance = ActiveShake.ShakeInstance;
		ShakeInfo.ShakeSource = ActiveShake.SourceComponent;
		ActiveCameraShakes.Add(ShakeInfo);
	}
}

void FCameraShakePreviewer::Update(float DeltaTime, bool bIsPlaying)
{
	LastDeltaTime = DeltaTime;
	LastScrubTime.Reset();

	if (!bIsPlaying)
	{
		ResetModifiers();
	}
}

void FCameraShakePreviewer::Scrub(float ScrubTime)
{
	LastDeltaTime.Reset();
	LastScrubTime = ScrubTime;

	ResetModifiers();
}

void FCameraShakePreviewer::ResetModifiers()
{
	LastLocationModifier = FVector::ZeroVector;
	LastRotationModifier = FRotator::ZeroRotator;
	LastFOVModifier = 0.f;

	LastPostProcessSettings.Reset();
	LastPostProcessBlendWeights.Reset();
}

void FCameraShakePreviewer::ModifyView(FEditorViewportViewModifierParams& Params)
{
	OnModifyView(Params);
}

void FCameraShakePreviewer::OnModifyView(FEditorViewportViewModifierParams& Params)
{
	FMinimalViewInfo& InOutPOV(Params.ViewInfo);
	const FMinimalViewInfo OriginalPOV(Params.ViewInfo);

	// This is a simpler version of what UCameraModifier_CameraShake does, with extra
	// support for scrubbing.
	if (LastDeltaTime.IsSet() || LastScrubTime.IsSet())
	{
		LastPostProcessSettings.Reset();
		LastPostProcessBlendWeights.Reset();

		for (FPreviewCameraShakeInfo& ActiveShake : ActiveShakes)
		{
			if (ActiveShake.ShakeInstance != nullptr)
			{
				float CurShakeAlpha = 1.f;

				if (ActiveShake.SourceComponent.IsValid())
				{
					const UCameraShakeSourceComponent* SourceComponent = ActiveShake.SourceComponent.Get();
					const float AttenuationFactor = SourceComponent->GetAttenuationFactor(InOutPOV.Location);
					CurShakeAlpha *= AttenuationFactor;
				}

				if (LastDeltaTime.IsSet())
				{
					ActiveShake.ShakeInstance->UpdateAndApplyCameraShake(LastDeltaTime.GetValue(), CurShakeAlpha, InOutPOV);
				}
				else if (LastScrubTime.IsSet())
				{
					float RelativeScrubTime = LastScrubTime.GetValue() - ActiveShake.StartTime;
					ActiveShake.ShakeInstance->ScrubAndApplyCameraShake(RelativeScrubTime, CurShakeAlpha, InOutPOV);
				}

				if (InOutPOV.PostProcessBlendWeight > 0.f)
				{
					Params.AddPostProcessBlend(InOutPOV.PostProcessSettings, InOutPOV.PostProcessBlendWeight);
					LastPostProcessSettings.Add(InOutPOV.PostProcessSettings);
					LastPostProcessBlendWeights.Add(InOutPOV.PostProcessBlendWeight);
				}
				InOutPOV.PostProcessSettings = FPostProcessSettings();
				InOutPOV.PostProcessBlendWeight = 0.f;
			}
		}

		LastLocationModifier = InOutPOV.Location - OriginalPOV.Location;
		LastRotationModifier = InOutPOV.Rotation - OriginalPOV.Rotation;
		LastFOVModifier = InOutPOV.FOV - OriginalPOV.FOV;

		LastDeltaTime.Reset();
		LastScrubTime.Reset();

		// Delete any obsolete shakes.
		for (int32 i = ActiveShakes.Num() - 1; i >= 0; i--)
		{
			const FPreviewCameraShakeInfo& ShakeInfo = ActiveShakes[i];
			if (ShakeInfo.ShakeInstance == nullptr || ShakeInfo.ShakeInstance->IsFinished() || ShakeInfo.SourceComponent.IsStale())
			{
				if (ShakeInfo.ShakeInstance != nullptr)
				{
					ShakeInfo.ShakeInstance->TeardownShake();
				}

				ActiveShakes.RemoveAt(i, 1);
			}
		}
	}
	else
	{
		InOutPOV.Location += LastLocationModifier;
		InOutPOV.Rotation += LastRotationModifier;
		InOutPOV.FOV += LastFOVModifier;

		for (int32 PPIndex = 0; PPIndex < LastPostProcessSettings.Num(); ++PPIndex)
		{
			Params.AddPostProcessBlend(LastPostProcessSettings[PPIndex], LastPostProcessBlendWeights[PPIndex]);
		}
	}
}

void FCameraShakePreviewer::RegisterViewModifier()
{
	if (GEditor == nullptr)
	{
		return;
	}

	// Register our view modifier on all appropriate viewports, and remember which viewports we did that on.
	// We will later make sure to unregister on the same list, except for any viewport that somehow disappeared since,
	// which we will be notified about with the OnLevelViewportClientListChanged event.
	RegisteredViewportClients.Reset();
	for (FLevelEditorViewportClient* LevelVC : GEditor->GetLevelViewportClients())
	{		
		if (LevelVC && LevelVC->AllowsCinematicControl() && LevelVC->GetViewMode() != VMI_Unknown)
		{
			RegisteredViewportClients.Add(LevelVC);
			LevelVC->ViewModifiers.AddRaw(this, &FCameraShakePreviewer::OnModifyView);
		}
	}

	GEditor->OnLevelViewportClientListChanged().AddRaw(this, &FCameraShakePreviewer::OnLevelViewportClientListChanged);
}

void FCameraShakePreviewer::UnRegisterViewModifier()
{
	if (GEditor == nullptr)
	{
		return;
	}

	GEditor->OnLevelViewportClientListChanged().RemoveAll(this);

	for (FLevelEditorViewportClient* ViewportClient : RegisteredViewportClients)
	{
		ViewportClient->ViewModifiers.RemoveAll(this);
	}
	RegisteredViewportClients.Reset();
}

void FCameraShakePreviewer::OnLevelViewportClientListChanged()
{
	if (GEditor != nullptr)
	{
		// If any viewports were removed while we were playing, simply get rid of them from our list of
		// registered viewports.
		TSet<FLevelEditorViewportClient*> PreviousViewportClients(RegisteredViewportClients);
		TSet<FLevelEditorViewportClient*> NewViewportClients(GEditor->GetLevelViewportClients());
		RegisteredViewportClients = PreviousViewportClients.Intersect(NewViewportClients).Array();
	}
}

void FCameraShakePreviewer::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (FPreviewCameraShakeInfo& ActiveShake : ActiveShakes)
	{
		if (ActiveShake.ShakeInstance)
		{
			Collector.AddReferencedObject(ActiveShake.ShakeInstance);
		}
	}
}

void FCameraShakePreviewer::OnObjectsReplaced(const TMap<UObject*, UObject*>& ReplacementMap)
{
	const bool bImmediately = true;
	for (int32 i = ActiveShakes.Num() - 1; i >= 0; i--)
	{
		FPreviewCameraShakeInfo& ActiveShake = ActiveShakes[i];
		if (ReplacementMap.Find(ActiveShake.ShakeInstance))
		{
			// If a camera shake gets recompiled, we just stop and discard it.
			ActiveShake.ShakeInstance->StopShake(bImmediately);
			ActiveShake.ShakeInstance->TeardownShake();
			ActiveShakes.RemoveAt(i);
		}
	}
}

#endif

