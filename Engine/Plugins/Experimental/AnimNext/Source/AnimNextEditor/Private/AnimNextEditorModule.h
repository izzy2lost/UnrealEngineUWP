// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IAnimNextEditorModule.h"

class FAnimNextGraphPanelNodeFactory;

namespace UE::AnimNext::Editor
{
	class FParamNamePropertyTypeIdentifier;
	class FParametersGraphPanelPinFactory;
	class FLocatorContext;
}

namespace UE::Workspace
{
	class IWorkspaceEditorModule;
}

namespace UE::AnimNext::Editor
{

class FModule : public IModule
{

private:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	// IAnimNextEditorModule interface
	virtual TSharedRef<SWidget> CreateParameterPicker(const FParameterPickerArgs& InArgs) override;
	virtual void RegisterLocatorFragmentEditorType(FName InLocatorFragmentEditorName) override;
	virtual void UnregisterLocatorFragmentEditorType(FName InLocatorFragmentEditorName) override;

private:
	void RegisterWorkspaceDocumentTypes(Workspace::IWorkspaceEditorModule& WorkspaceEditorModule);
	void UnregisterWorkspaceDocumentTypes();

private:
	/** Node factory for the AnimNext graph */
	TSharedPtr<FAnimNextGraphPanelNodeFactory> AnimNextGraphPanelNodeFactory;
	
	/** Pin factory for parameters */
	TSharedPtr<FParametersGraphPanelPinFactory> ParametersGraphPanelPinFactory;

	/** Type identifier for parameter names */
	TSharedPtr<FParamNamePropertyTypeIdentifier> Identifier;

	/** Registered names for locator fragments */
	TSet<FName> LocatorFragmentEditorNames;

	friend class FLocatorContext;
};

}
