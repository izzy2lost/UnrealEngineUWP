// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetRegistry/AssetData.h"
#include "MuCO/CustomizableObjectPrivate.h"
#include "MuCO/CustomizableObjectCompilerTypes.h"
#include "MuCO/UnrealToMutableTextureConversionUtils.h"
#include "MuCOE/CompilationMessageCache.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSource.h"
#include "MuCOE/CustomizableObjectEditorLogger.h"
#include "UObject/GCObject.h"
#include "TickableEditorObject.h"

#include "Framework/Notifications/NotificationManager.h"

class FCustomizableObjectCompileRunnable;
class FCustomizableObjectSaveDDRunnable;
class FReferenceCollector;
class FRunnableThread;
class FText;
class UCustomizableObject;
class UCustomizableObjectNode;

class FCustomizableObjectCompiler : public FTickableEditorObject, public FTickableCookObject, public FGCObject
{
public:
	virtual ~FCustomizableObjectCompiler() override {}

	/** Check for pending compilation process. Returns true if an object has been updated. */
	bool Tick(bool bBlocking = false);
	int32 GetNumRemainingWork() const;

	// FTickableGameObject interface
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; };
	virtual bool IsTickable() const override;
	virtual void Tick( float InDeltaTime ) override;
	virtual TStatId GetStatId() const override;

	// FTickableCookObject interface
	virtual void TickCook(float DeltaTime, bool bCookCompete) override;


	/** Generate the Mutable Graph from the Unreal Graph. */
	mu::NodePtr Export(UCustomizableObject* Object, const FCompilationOptions& Options, TArray<TSoftObjectPtr<const UTexture>>& OutRuntimeReferencedTextures, TArray<FMutableSourceTextureData>& OutCompilerReferencedTextures);

	void CompilerLog(const FText& Message, const TArray<const UObject*>& UObject, const EMessageSeverity::Type MessageSeverity = EMessageSeverity::Warning, const bool bAddBaseObjectInfo = true, const ELoggerSpamBin SpamBin = ELoggerSpamBin::ShowAll);
	void CompilerLog(const FText& Message, const UObject* Context = nullptr, const EMessageSeverity::Type MessageSeverity = EMessageSeverity::Warning, const bool bAddBaseObjectInfo = true, const ELoggerSpamBin SpamBin = ELoggerSpamBin::ShowAll);
	void NotifyCompilationErrors() const;

	void FinishCompilationTask();
	void FinishSavingDerivedDataTask();

	void ForceFinishCompilation();
	void ClearCompileRequests();

	void AddCompileNotification(const FText& CompilationStep) const;
	static void RemoveCompileNotification();

	/** Load required assets and compile.
	 *
	 * Loads assets which reference Object's package asynchronously before calling ProcessChildObjectsRecursively. */
	void Compile(const TSharedRef<FCompilationRequest>& InCompileRequest);
	void Compile(const TArray<TSharedRef<FCompilationRequest>>& InCompileRequests);
	
	bool IsRequestQueued(const TSharedRef<FCompilationRequest>& InCompileRequest) const;

	/** FSerializableObject interface */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FCustomizableObjectCompiler");
	}

	/** Simply add CO elements from ArrayAssetData to ArrayGCProtect when they've been loaded from ArrayAssetData */
	void UpdateArrayGCProtect();

private:

	// Object containing all error and warning logs raised during compilation.
	FCompilationMessageCache CompilationLogsContainer;

	void SetCompilationState(ECompilationStatePrivate State, ECompilationResultPrivate Result) const;
	
	void CompileInternal(bool bAsync = false);

	void CompleteRequest(ECompilationStatePrivate State, ECompilationResultPrivate Result);
	bool TryPopCompileRequest();

	void PreloadingReferencerAssetsCallback(bool bAsync);
	
	void ProcessChildObjectsRecursively(UCustomizableObject* Object, FMutableGraphGenerationContext &GenerationContext);
	
	// Will output to Mutable Log the warning and error messages generated during the CO compilation
	// and update the values of NumWarnings and NumErrors
	void UpdateCompilerLogData();

	/** If duplicated elements are found in each entry of ParameterNamesMap, a warning for
	the parameters with repeated name will be generated */
	void DisplayParameterWarning(FMutableGraphGenerationContext& GenerationContext);
	
	/** If duplicated node ids are found, usually due to duplicating CustomizableObjects Assets, a warning
	for the nodes with repeated ids will be generated */
	void DisplayDuplicatedNodeIdsWarning(FMutableGraphGenerationContext& GenerationContext);
	
	/** Display warnings for unnamed node objects */
	void DisplayUnnamedNodeObjectWarning(FMutableGraphGenerationContext& GenerationContext);
	
	/** Display a warning for each node contains an orphan pin. */
	void DisplayOrphanNodesWarning(FMutableGraphGenerationContext& GenerationContext);
	
	mu::NodeObjectPtr GenerateMutableRoot(UCustomizableObject* Object, FMutableGraphGenerationContext& GenerationContext, FText& ErrorMessage, bool& bOutIsRootObject);


	/** Launches the compile task in another thread when compiling a CO in the editor
	* @param bShowNotification [in] whether to show the compiling CO notification or not
	* @return nothing */
	void LaunchMutableCompile();

	/** Launches the save derived data task in another thread after compiling a CO in the
	* editor
	* @param bShowNotification [in] whether to show the saving DD notification or not
	* @return nothing */
	void SaveCODerivedData();

	/** Add to ArrayAssetData the FAssetData information of all referencers of static class type UCustomizableObject::StaticClass()
	* that reference the package given by the PathName parameter
	* @param PathName            [in]  path to the CO to be analyzed (for instance, CO->GetOuter()->GetPathName())
	* @param ArrayReferenceNames [out] array with the package names which are referenced by the CO with PathName given as parameter
	* @return nothing */
	void AddCachedReferencers(const FName& PathName, TArray<FName>& ArrayReferenceNames);

	/** Just used to clean ArrayAssetData
	* @return nothing */
	void CleanCachedReferencers();

	/** Will test if package path given by PackageName parameter is one of ArrayAssetData's elements FAssetData::PackageName value
	* @param PackageName [in] package name to test
	* @return true if cached, false otherwise */
	bool IsCachedInAssetData(const FString& PackageName);

	/** Find FAssetData ArrayAssetData with PackageName given by parameter
	* @param PackageName [in] package name to find in ArrayAssetData
	* @return pointer to element if any found, nullptr otherwise */
	FAssetData* GetCachedAssetData(const FString& PackageName);

	ECompilationResultPrivate GetCompilationResult() const;

	/** Pointer to the Asynchronous Preloading process call back */
	TSharedPtr<FStreamableHandle> AsynchronousStreamableHandlePtr;
	TArray<FSoftObjectPath> ArrayAssetToStream;

	/** Compile task and thread. */
	TSharedPtr<FCustomizableObjectCompileRunnable> CompileTask;
	TSharedPtr<FRunnableThread> CompileThread;

	/** SaveDD task and thread. */
	TSharedPtr<FCustomizableObjectSaveDDRunnable> SaveDDTask;
	TSharedPtr<FRunnableThread> SaveDDThread;

	/** Array where to put the names of the already processed child in ProcessChildObjectsRecursively */
	TArray<FName> ArrayAlreadyProcessedChild;

	/** Array with all the packages used to compile current Customizable Object */
	TArray<FAssetData> ArrayAssetData;

	/** Array used to protect from garbage collection those COs loaded asynchronously */
	TArray<TObjectPtr<UCustomizableObject>> ArrayGCProtect;

	TSharedPtr<mu::Model, ESPMode::ThreadSafe> CurrentModel;

	// Protected from GC with FCustomizableObjectCompiler::AddReferencedObjects
	TObjectPtr<UCustomizableObject> CurrentObject = nullptr;

	FCompilationOptions CurrentOptions;

	/** Current Compilation request. */
	TSharedPtr<FCompilationRequest> CurrentRequest;

	/** Pending requests. */
	TArray<TSharedRef<FCompilationRequest>> CompileRequests;

	uint32 NumCompilationRequests = 0;

	/** Compilation progress bar handle */
	FProgressNotificationHandle CompileNotificationHandle;

	/** Compilation start time in seconds. */
	double CompilationStartTime = 0;

};
