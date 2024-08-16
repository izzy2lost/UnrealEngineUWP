// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_ForceFeedbackEffect.h"

#include "AssetViewUtils.h"
#include "ContentBrowserMenuContexts.h"
#include "Framework/Application/SlateApplication.h"
#include "ObjectEditorUtils.h"
#include "ToolMenus.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"

#define LOCTEXT_NAMESPACE "AssetDefinition_ForceFeedbackEffect"

static FPreviewForceFeedbackEffect PreviewForceFeedbackEffect = FPreviewForceFeedbackEffect();

FText UAssetDefinition_ForceFeedbackEffect::GetAssetDisplayName() const
{
	return LOCTEXT("AssetTypeActions_ForceFeedbackEffect", "Force Feedback Effect");
}

FLinearColor UAssetDefinition_ForceFeedbackEffect::GetAssetColor() const
{
	return FColor(175, 0, 0);
}

TSoftClassPtr<UObject> UAssetDefinition_ForceFeedbackEffect::GetAssetClass() const
{
	return UForceFeedbackEffect::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_ForceFeedbackEffect::GetAssetCategories() const
{
	static FAssetCategoryPath Categories[] =
		{
		EAssetCategoryPaths::Input,
		};

	return Categories;
}

// Menu Extensions
// --------------------------------------------------------------------

namespace MenuExtension_ForceFeedbackEffect
{
	bool IsEffectPlaying(const TArray<TWeakObjectPtr<UForceFeedbackEffect>>& Objects)
	{
		if (PreviewForceFeedbackEffect.ForceFeedbackEffect)
		{
			for (const TWeakObjectPtr<UForceFeedbackEffect>& EffectPtr : Objects)
			{
				UForceFeedbackEffect* Effect = EffectPtr.Get();
				if (Effect && PreviewForceFeedbackEffect.ForceFeedbackEffect == Effect)
				{
					return true;
				}
			}
		}

		return false;
	}

	bool IsEffectPlaying(const UForceFeedbackEffect* ForceFeedbackEffect)
	{
		return PreviewForceFeedbackEffect.ForceFeedbackEffect && PreviewForceFeedbackEffect.ForceFeedbackEffect == ForceFeedbackEffect;
	}

	bool IsEffectPlaying(const FAssetData& AssetData)
	{
		if (PreviewForceFeedbackEffect.ForceFeedbackEffect)
		{
			if (PreviewForceFeedbackEffect.ForceFeedbackEffect->GetFName() == AssetData.AssetName)
			{
				if (PreviewForceFeedbackEffect.ForceFeedbackEffect->GetOutermost()->GetFName() == AssetData.PackageName)
				{
					return true;
				}
			}
		}

		return false;
	}

	void StopEffect() 
	{
		PreviewForceFeedbackEffect.ResetDeviceProperties();
		PreviewForceFeedbackEffect.ForceFeedbackEffect = nullptr;

		if (IInputInterface* InputInterface = FSlateApplication::Get().GetInputInterface())
		{
			InputInterface->SetForceFeedbackChannelValues(0, FForceFeedbackValues());
		}
	}

	void PlayEffect(UForceFeedbackEffect* Effect)
	{
		if (Effect)
		{
			PreviewForceFeedbackEffect.ForceFeedbackEffect = Effect;
			PreviewForceFeedbackEffect.PlayTime = 0.f;
			PreviewForceFeedbackEffect.PlatformUser = IPlatformInputDeviceMapper::Get().GetPrimaryPlatformUser();
			PreviewForceFeedbackEffect.ActivateDeviceProperties();
		}
		else
		{
			StopEffect();
		}
	}

	bool CanExecutePlayCommand(TSharedPtr<TArray<FAssetData>> InAssetsData)
	{
		return InAssetsData.IsValid() && InAssetsData->Num() == 1;
	}

	void ExecutePlayEffect(const TArray<TWeakObjectPtr<UForceFeedbackEffect>>& Objects)
	{
		for (const TWeakObjectPtr<UForceFeedbackEffect>& EffectPtr : Objects)
		{
			if (UForceFeedbackEffect* Effect = EffectPtr.Get())
			{
				// Only play the first valid effect
				PlayEffect(Effect);
				break;
			}
		}
	}

	void ExecutePlayEffect(TSharedPtr<TArray<FAssetData>> InAssetsData)
	{
		TArray<UObject*> SelectedForceFeedbackEffectObjects;
		AssetViewUtils::FLoadAssetsSettings Settings{
			// Default settings
		};

		if (InAssetsData.IsValid())
		{
			AssetViewUtils::LoadAssetsIfNeeded(*InAssetsData.Get(), SelectedForceFeedbackEffectObjects, Settings);
		}

		const TArray<TWeakObjectPtr<UForceFeedbackEffect>>& Objects = FObjectEditorUtils::GetTypedWeakObjectPtrs<UForceFeedbackEffect>(SelectedForceFeedbackEffectObjects);
		ExecutePlayEffect(Objects);
	}

	void ExecuteStopEffect()
	{
		StopEffect();
	}

	static FDelayedAutoRegisterHelper DelayedAutoRegister(EDelayedRegisterRunPhase::EndOfEngineInit, []{ 
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
		{
			FToolMenuOwnerScoped OwnerScoped(UE_MODULE_NAME);
			UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UForceFeedbackEffect::StaticClass());

			FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");
			Section.AddDynamicEntry(NAME_None, FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
			{
				if (const UContentBrowserAssetContextMenuContext* CBContext = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InSection))
				{
					const TArray<FAssetData> SelectedForceFeedbackEffects = CBContext->GetSelectedAssetsOfType(UForceFeedbackEffect::StaticClass(), EIncludeSubclasses::No);
					if (!SelectedForceFeedbackEffects.IsEmpty())
					{
						TSharedPtr<TArray<FAssetData>> SelectedForceFeedbackEffectsPtr = MakeShared<TArray<FAssetData>>(SelectedForceFeedbackEffects);
						InSection.AddMenuEntry(
							TEXT("ForceFeedbackEffect_PlayEffect"),
							LOCTEXT("ForceFeedbackEffect_PlayEffect", "Play"),
							LOCTEXT("ForceFeedbackEffect_PlayEffectTooltip", "Plays the selected force feedback effect."),
							FSlateIcon(FAppStyle::GetAppStyleSetName(), "ContentBrowser.AssetAction.PlayIcon"),
							FUIAction(
								FExecuteAction::CreateStatic(&ExecutePlayEffect, SelectedForceFeedbackEffectsPtr),
								FCanExecuteAction::CreateStatic(&CanExecutePlayCommand, SelectedForceFeedbackEffectsPtr)
								)
							);

						InSection.AddMenuEntry(
							TEXT("ForceFeedbackEffect_StopEffect"),
							LOCTEXT("ForceFeedbackEffect_StopEffect", "Stop"),
							LOCTEXT("ForceFeedbackEffect_StopEffectTooltip", "Stops the selected force feedback effect."),
							FSlateIcon(FAppStyle::GetAppStyleSetName(), "ContentBrowser.AssetAction.StopIcon"),
							FUIAction(
								FExecuteAction::CreateStatic(&ExecuteStopEffect),
								FCanExecuteAction()
								)
							);
					}
				}
			}));
		}));
	});
}

// --------------------------------------------------------------------
// Menu Extensions

EAssetCommandResult UAssetDefinition_ForceFeedbackEffect::ActivateAssets(const FAssetActivateArgs& InActivateArgs) const
{
	if (InActivateArgs.ActivationMethod == EAssetActivationMethod::Previewed)
	{
		TArray<UObject*> Objects = InActivateArgs.LoadObjects<UObject>();
		for (UObject* Object : Objects)
		{
			if (UForceFeedbackEffect* TargetEffect = Cast<UForceFeedbackEffect>(Object))
			{
				// Only target the first valid effect
				TArray<TWeakObjectPtr<UForceFeedbackEffect>> EffectList;
				EffectList.Add(MakeWeakObjectPtr(TargetEffect));
				if (MenuExtension_ForceFeedbackEffect::IsEffectPlaying(EffectList))
				{
					MenuExtension_ForceFeedbackEffect::ExecuteStopEffect();
				}
				else
				{
					MenuExtension_ForceFeedbackEffect::ExecutePlayEffect(EffectList);
				}

				return EAssetCommandResult::Handled;
			}
		}
	}
	return EAssetCommandResult::Unhandled;
}

bool UAssetDefinition_ForceFeedbackEffect::GetThumbnailActionOverlay(const FAssetData& InAssetData, FAssetActionThumbnailOverlayInfo& OutActionOverlayInfo) const
{
	auto OnGetDisplayBrushLambda = [this, InAssetData]() -> const FSlateBrush*
	{
		if (MenuExtension_ForceFeedbackEffect::IsEffectPlaying(InAssetData))
		{
			return FAppStyle::GetBrush("ContentBrowser.AssetAction.StopIcon");
		}
		return FAppStyle::GetBrush("ContentBrowser.AssetAction.PlayIcon");
	};

	OutActionOverlayInfo.ActionImageWidget = SNew(SImage).Image_Lambda(OnGetDisplayBrushLambda);

	auto OnClickedLambda = [InAssetData]() -> FReply
	{
		if (MenuExtension_ForceFeedbackEffect::IsEffectPlaying(InAssetData))
		{
			MenuExtension_ForceFeedbackEffect::StopEffect();
		}
		else
		{
			// Load and play asset
			MenuExtension_ForceFeedbackEffect::PlayEffect(Cast<UForceFeedbackEffect>(InAssetData.GetAsset()));
		}
		return FReply::Handled();
	};

	auto OnToolTipTextLambda = [InAssetData]() -> FText
	{
		if (MenuExtension_ForceFeedbackEffect::IsEffectPlaying(InAssetData))
		{
			return LOCTEXT("Thumbnail_StopForceFeedbackToolTip", "Stop selected force feedback effect");
		}
		return LOCTEXT("Thumbnail_PlayForceFeedbackToolTip", "Play selected force feedback effect");
	};

	OutActionOverlayInfo.ActionButtonWidget = SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
		.ContentPadding(0.0f)
		.ToolTipText_Lambda(OnToolTipTextLambda)
		.OnClicked_Lambda(OnClickedLambda)
		[
			SNew(SImage)
			.Image_Lambda(OnGetDisplayBrushLambda)
		];

	return true;
}

bool FPreviewForceFeedbackEffect::IsTickable() const
{
	return (ForceFeedbackEffect != nullptr);
}

void FPreviewForceFeedbackEffect::Tick( float DeltaTime )
{
	FForceFeedbackValues ForceFeedbackValues;

	if (!Update(DeltaTime, ForceFeedbackValues))
	{
		ResetDeviceProperties();
		ForceFeedbackEffect = nullptr;
	}

	IInputInterface* InputInterface = FSlateApplication::Get().GetInputInterface();
	if (InputInterface)
	{
		InputInterface->SetForceFeedbackChannelValues(0, ForceFeedbackValues);
	}
}

TStatId FPreviewForceFeedbackEffect::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FPreviewForceFeedbackEffect, STATGROUP_Tickables);
}

void FPreviewForceFeedbackEffect::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject(ForceFeedbackEffect);
}

#undef LOCTEXT_NAMESPACE
