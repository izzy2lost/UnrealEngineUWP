// Copyright Epic Games, Inc. All Rights Reserved.

#include "PropertyAnimatorCoreEditorModule.h"

#include "Animators/PropertyAnimatorCoreBase.h"
#include "Customizations/PropertyAnimatorCoreEditorContextTypeCustomization.h"
#include "Customizations/PropertyAnimatorCoreEditorDetailCustomization.h"
#include "Customizations/PropertyAnimatorCoreEditorSequencerTimeSourceChannelTypeCustomization.h"
#include "ISequencerModule.h"
#include "Modules/ModuleManager.h"
#include "Properties/PropertyAnimatorCoreContext.h"
#include "PropertyEditorModule.h"
#include "Sequencer/MovieSceneAnimatorTrackEditor.h"
#include "TimeSources/PropertyAnimatorCoreSequencerTimeSource.h"

void FPropertyAnimatorCoreEditorModule::StartupModule()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomPropertyTypeLayout(UPropertyAnimatorCoreContext::StaticClass()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FPropertyAnimatorCoreEditorContextTypeCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(FPropertyAnimatorCoreSequencerTimeSourceChannel::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FPropertyAnimatorCoreEditorSequencerTimeSourceChannelTypeCustomization::MakeInstance));
	PropertyModule.RegisterCustomClassLayout(UPropertyAnimatorCoreBase::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FPropertyAnimatorCoreEditorDetailCustomization::MakeInstance));

	ISequencerModule& SequencerModule = FModuleManager::Get().LoadModuleChecked<ISequencerModule>("Sequencer");
	AnimatorTrackCreateEditorHandle = SequencerModule.RegisterTrackEditor(FOnCreateTrackEditor::CreateStatic(&FMovieSceneAnimatorTrackEditor::CreateTrackEditor));
}

void FPropertyAnimatorCoreEditorModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomPropertyTypeLayout(UPropertyAnimatorCoreContext::StaticClass()->GetFName());
		PropertyModule.UnregisterCustomPropertyTypeLayout(FPropertyAnimatorCoreSequencerTimeSourceChannel::StaticStruct()->GetFName());
		PropertyModule.UnregisterCustomClassLayout(UPropertyAnimatorCoreBase::StaticClass()->GetFName());
	}

	if (FModuleManager::Get().IsModuleLoaded("Sequencer"))
	{
		ISequencerModule& SequencerModule = FModuleManager::GetModuleChecked<ISequencerModule>("Sequencer");
		SequencerModule.UnRegisterTrackEditor(AnimatorTrackCreateEditorHandle);
		AnimatorTrackCreateEditorHandle.Reset();
	}
}

IMPLEMENT_MODULE(FPropertyAnimatorCoreEditorModule, PropertyAnimatorCoreEditor)
