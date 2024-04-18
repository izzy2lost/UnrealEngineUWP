// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"
#include "Templates/SharedPointer.h"

struct FBakingConfiguration;
class FCustomizableObjectEditorLogger;
class ICustomizableObjectDebugger;
class ICustomizableObjectEditor;
class ICustomizableObjectInstanceEditor;
class IToolkitHost;
class UCustomizableObject;
class UCustomizableObjectPrivate;
class UCustomizableObjectInstance;
class FExtensibilityManager;
class FCustomizableObjectCompilerBase;
class FBakeOperationCompletedDelegate;

extern const FName CustomizableObjectEditorAppIdentifier;
extern const FName CustomizableObjectInstanceEditorAppIdentifier;
extern const FName CustomizableObjectPopulationEditorAppIdentifier;
extern const FName CustomizableObjectPopulationClassEditorAppIdentifier;
extern const FName CustomizableObjectDebuggerAppIdentifier;

/**
 * Customizable object editor module interface
 */
class ICustomizableObjectEditorModule : public IModuleInterface
{
public:
	static ICustomizableObjectEditorModule* Get()
	{
		return FModuleManager::LoadModulePtr<ICustomizableObjectEditorModule>("CustomizableObjectEditor");
	}
	
	static ICustomizableObjectEditorModule& GetChecked()
	{
		return FModuleManager::LoadModuleChecked<ICustomizableObjectEditorModule>("CustomizableObjectEditor");
	}

	virtual TSharedPtr<FExtensibilityManager> GetCustomizableObjectEditorToolBarExtensibilityManager() { return nullptr; }
	virtual TSharedPtr<FExtensibilityManager> GetCustomizableObjectEditorMenuExtensibilityManager() { return nullptr; }
	
	/** Returns the module logger. */
	virtual FCustomizableObjectEditorLogger& GetLogger() = 0;
	
	/** Return if the CO is not compiled or the ParticipatingObjects system has detected a change (participating objects dirty or re-saved since last compilation).
	  * @param Object object to check.
	  * @param OutOfDatePackages list of out of date packages.
   	  * @return true if the compilation is out of date. */
	virtual bool IsCompilationOutOfDate(const UCustomizableObject& Object, TArray<FName>* OutOfDatePackages = nullptr) const = 0;

	/** Create a new compiler. */
	virtual TSharedRef<FCustomizableObjectCompilerBase> CreateCompiler() const = 0;

	/** See GraphTraversal::IsRootObject(...) */
	virtual bool IsRootObject(const UCustomizableObject& Object) const = 0;
	
	/**
	 * Execute this method in order to bake the provided instance. It will schedule a special type of instance update before proceeding with the bake itself.
	 * @param InTargetInstance The instance we want to bake
	 * @param InBakingConfig Structure containing the configuration to be used for the baking
	 */
	virtual void BakeCustomizableObjectInstance(UCustomizableObjectInstance* InTargetInstance, const FBakingConfiguration& InBakingConfig)  = 0;
};
 
