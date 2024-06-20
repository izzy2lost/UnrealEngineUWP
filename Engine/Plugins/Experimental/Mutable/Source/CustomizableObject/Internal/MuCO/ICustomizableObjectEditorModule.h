// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"
#include "Templates/SharedPointer.h"

class FCustomizableObjectEditorLogger;
class ICustomizableObjectDebugger;
class ICustomizableObjectEditor;
class ICustomizableObjectInstanceEditor;
class IToolkitHost;
class UCustomizableObject;
class UCustomizableObjectPrivate;
class UCustomizableObjectInstance;
class FExtensibilityManager;
class FBakeOperationCompletedDelegate;
struct FBakingConfiguration;
struct FCompilationRequest;

extern const FName CustomizableObjectEditorAppIdentifier;
extern const FName CustomizableObjectInstanceEditorAppIdentifier;
extern const FName CustomizableObjectPopulationEditorAppIdentifier;
extern const FName CustomizableObjectPopulationClassEditorAppIdentifier;
extern const FName CustomizableObjectDebuggerAppIdentifier;

/**
 * Customizable object editor module interface
 */
class CUSTOMIZABLEOBJECT_API ICustomizableObjectEditorModule : public IModuleInterface
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

	/** See GraphTraversal::IsRootObject(...) */
	virtual bool IsRootObject(const UCustomizableObject& Object) const = 0;

	/** Get the current VersionBridge's version for Object. 
	  * @return Current version as string. */
	virtual FString GetCurrentContentVersionForObject(const UCustomizableObject& Object) const = 0;

	/**
	 * Execute this method in order to bake the provided instance. It will schedule a special type of instance update before proceeding with the bake itself.
	 * @param InTargetInstance The instance we want to bake
	 * @param InBakingConfig Structure containing the configuration to be used for the baking
	 */
	virtual void BakeCustomizableObjectInstance(UCustomizableObjectInstance* InTargetInstance, const FBakingConfiguration& InBakingConfig) = 0;

	/** Request for a given customizable object to be compiled. Async compile requests will be queued and processed sequentially.
	 * @param InCompilationRequest - Request to compile an object.
	 * @param bForceRequest - Queue request even if already in the pending list. */
	virtual void CompileCustomizableObject(const TSharedRef<FCompilationRequest>& InCompilationRequest, bool bForceRequest = false) = 0;
	virtual void CompileCustomizableObjects(const TArray<TSharedRef<FCompilationRequest>>& InCompilationRequests, bool bForceRequests = false) = 0;

	virtual int32 Tick(bool bBlocking) = 0;

	/** Force finish current compile request and cancels all pending requests */
	virtual void CancelCompileRequests() = 0;

};
 
