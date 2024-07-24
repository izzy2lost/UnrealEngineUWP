// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MassCommonTypes.h"
#include "MassEntityTemplate.h"
#include "MassTranslator.h"
#include "MassEntityTemplateRegistry.generated.h"

#define ENSURE_SUPPORTED_TRAIT_OPERATION() ensureMsgf(bBuildInProgress == false, TEXT("This method is not expected to be called as "\
	"part of trait's BuildTemplate call. Traits are not supposed to add elements based on other traits due to arbitrary trait ordering."));

class UWorld;
class UMassEntityTraitBase;

enum class EFragmentInitialization : uint8
{
	DefaultInitializer,
	NoInitializer
};

struct FMassEntityTemplateBuildContext
{
	explicit FMassEntityTemplateBuildContext(FMassEntityTemplateData& InTemplate, FMassEntityTemplateID InTemplateID = FMassEntityTemplateID())
		: TemplateData(InTemplate)
		, TemplateID(InTemplateID)
	{}

	void SetTemplateName(const FString& Name)
	{
		TemplateData.SetTemplateName(Name);
	}

	//----------------------------------------------------------------------//
	// Fragments 
	//----------------------------------------------------------------------//
	template<typename T>
	T& AddFragment_GetRef()
	{
		TypeAdded(*T::StaticStruct());
		return TemplateData.AddFragment_GetRef<T>();
	}

	template<typename T>
	void AddFragment()
	{
		TypeAdded(*T::StaticStruct());
		TemplateData.AddFragment<T>();
	}

	void AddFragment(FConstStructView InFragment)
	{ 
		checkf(InFragment.GetScriptStruct(), TEXT("Expecting a valid fragment type"));
		TypeAdded(*InFragment.GetScriptStruct());
		TemplateData.AddFragment(InFragment);
	}

	template<typename T>
	void AddTag()
	{
		// Tags can be added by multiple traits, so they do not follow the same rules as fragments
		TemplateData.AddTag<T>();
		TypeAdded(*T::StaticStruct());
	}

	void AddTag(const UScriptStruct& TagType)
	{
		// Tags can be added by multiple traits, so they do not follow the same rules as fragments
		TemplateData.AddTag(TagType);
		TypeAdded(TagType);
	}

	template<typename T>
	void AddChunkFragment()
	{
		TypeAdded(*T::StaticStruct());
		TemplateData.AddChunkFragment<T>();
	}

	void AddConstSharedFragment(const FConstSharedStruct& InSharedFragment)
	{
		checkf(InSharedFragment.GetScriptStruct(), TEXT("Expecting a valide shared fragment type"));
		TypeAdded(*InSharedFragment.GetScriptStruct());
		TemplateData.AddConstSharedFragment(InSharedFragment);
	}

	void AddSharedFragment(const FSharedStruct& InSharedFragment)
	{
		checkf(InSharedFragment.GetScriptStruct(), TEXT("Expecting a valide shared fragment type"));
		TypeAdded(*InSharedFragment.GetScriptStruct());
		TemplateData.AddSharedFragment(InSharedFragment);
	}

	template<typename T>
	T* GetFragment()
	{
		return TemplateData.GetMutableFragment<T>();
	}

	template<typename T>
	bool HasFragment() const
	{
		ENSURE_SUPPORTED_TRAIT_OPERATION();
		return TemplateData.HasFragment<T>();
	}
	
	bool HasFragment(const UScriptStruct& ScriptStruct) const
	{
		ENSURE_SUPPORTED_TRAIT_OPERATION();
		return TemplateData.HasFragment(ScriptStruct);
	}

	template<typename T>
	bool HasTag() const
	{
		return TemplateData.HasTag<T>();
	}

	template<typename T>
	bool HasChunkFragment() const
	{
		ENSURE_SUPPORTED_TRAIT_OPERATION();
		return TemplateData.HasChunkFragment<T>();
	}

	template<typename T>
	bool HasSharedFragment() const
	{
		ENSURE_SUPPORTED_TRAIT_OPERATION();
		return TemplateData.HasSharedFragment<T>();
	}

	bool HasSharedFragment(const UScriptStruct& ScriptStruct) const
	{
		ENSURE_SUPPORTED_TRAIT_OPERATION();
		return TemplateData.HasSharedFragment(ScriptStruct);
	}

	template<typename T>
	bool HasConstSharedFragment() const
	{
		ENSURE_SUPPORTED_TRAIT_OPERATION();
		return TemplateData.HasConstSharedFragment<T>();
	}

	bool HasConstSharedFragment(const UScriptStruct& ScriptStruct) const
	{
		ENSURE_SUPPORTED_TRAIT_OPERATION();
		return TemplateData.HasConstSharedFragment(ScriptStruct);
	}

	//----------------------------------------------------------------------//
	// Translators
	//----------------------------------------------------------------------//
	template<typename T>
	void AddTranslator()
	{
		TypeAdded(*T::StaticClass());
		GetDefault<T>()->AppendRequiredTags(TemplateData.GetMutableTags());
	}

	//----------------------------------------------------------------------//
	// Dependencies
	//----------------------------------------------------------------------//
	template<typename T>
	void RequireFragment()
	{
		AddDependency(T::StaticStruct());
	}

	template<typename T>
	void RequireTag()
	{
		AddDependency(T::StaticStruct());
	}

	void AddDependency(const UStruct* Dependency)
	{
		TraitsData.Last().TypesRequired.Add(Dependency);
	}

	//----------------------------------------------------------------------//
	// Template access
	//----------------------------------------------------------------------//
	FMassEntityTemplateID GetTemplateID() const { return TemplateID; }
	TArray<FMassEntityTemplateData::FObjectFragmentInitializerFunction>& GetMutableObjectFragmentInitializers() { return TemplateData.GetMutableObjectFragmentInitializers(); }

	//----------------------------------------------------------------------//
	// Build methods
	//----------------------------------------------------------------------//

	/**
	 * Builds context from a list of traits
	 * @param Traits is the list of all the traits to build an entity
	 * @param World owning the MassEntitySubsystem for which the entity template is built
	 * @return true if there were no validation errors
	 */
	bool BuildFromTraits(TConstArrayView<UMassEntityTraitBase*> Traits, const UWorld& World);

protected:

	/**
	 * Validate the build context for fragment trait ownership and trait fragment missing dependency
	 * @param World owning the MassEntitySubsystem for which the entity template is validated against
	 * @return true if there were no validation errors
	 */
	bool ValidateBuildContext(const UWorld& World);

	void TypeAdded(const UStruct& Type)
	{
		checkf(TraitsData.Num(), TEXT("Adding elements to the build context before BuildFromTraits or SetTraitBeingProcessed was called is unsupported"));
		TraitsData.Last().TypesAdded.Add(&Type);
	}

	/** 
	 * Return true if the given trait can be used. The function will fail if a trait instance of the given class has already 
	 * been processed. The function will also fail the very same trait instance is used multiple times.
	 * Note that it's ok for Trait to be nullptr to indicate the subsequent additions to the build context are procedural
	 * in nature and are not associated with any traits. In that case it's ok to have multiple SetTraitBeingProcessed(nullptr)
	 * calls.
	 */
	bool SetTraitBeingProcessed(const UMassEntityTraitBase* Trait);

	void ResetBuildTimeData()
	{
		TraitsData.Reset();
		TraitsProcessed.Reset();
		IgnoredTraits.Reset();
		bBuildInProgress = false;
	}

	struct FTraitData
	{
		const UMassEntityTraitBase* Trait = nullptr;
		TArray<const UStruct*> TypesAdded;
		TArray<const UStruct*> TypesRequired;
	};
	TArray<FTraitData> TraitsData;
	TSet<const UMassEntityTraitBase*> TraitsProcessed;
	TSet<const UMassEntityTraitBase*> IgnoredTraits;
	bool bBuildInProgress = false;

	FMassEntityTemplateData& TemplateData;
	FMassEntityTemplateID TemplateID;
};

/** @todo document 
 */
struct MASSSPAWNER_API FMassEntityTemplateRegistry
{
	// @todo consider TFunction instead
	DECLARE_DELEGATE_ThreeParams(FStructToTemplateBuilderDelegate, const UWorld* /*World*/, const FConstStructView /*InStructInstance*/, FMassEntityTemplateBuildContext& /*BuildContext*/);

	explicit FMassEntityTemplateRegistry(UObject* InOwner = nullptr);

	/** Initializes and stores the EntityManager the templates will be associated with. Needs to be called before any template operations.
	 *  Note that the function will only let users set the EntityManager once. Once it's set the subsequent calls will
	 *  have no effect. If attempting to set a different EntityManaget an ensure will trigger. */
	void Initialize(const TSharedPtr<FMassEntityManager>& InEntityManager);

	void ShutDown();

	UWorld* GetWorld() const;

	static FStructToTemplateBuilderDelegate& FindOrAdd(const UScriptStruct& DataType);

	/** Removes all the cached template instances */
	void DebugReset();

	const TSharedRef<FMassEntityTemplate>* FindTemplateFromTemplateID(FMassEntityTemplateID TemplateID) const;

	/**
	 * Adds a template based on TemplateData
	 */
	const TSharedRef<FMassEntityTemplate>& FindOrAddTemplate(FMassEntityTemplateID TemplateID, FMassEntityTemplateData&& TemplateData);

	UE_DEPRECATED(5.3, "We no longer support fething mutable templates from the TemplateRegistry. Stored templates are considered const.")
	FMassEntityTemplate* FindMutableTemplateFromTemplateID(FMassEntityTemplateID TemplateID);

	UE_DEPRECATED(5.3, "CreateTemplate is no longer available. Use AddTemplate instead.")
	FMassEntityTemplate& CreateTemplate(const uint32 HashLookup, FMassEntityTemplateID TemplateID);

	void DestroyTemplate(FMassEntityTemplateID TemplateID);

	UE_DEPRECATED(5.3, "InitializeEntityTemplate is no longer available. Use AddTemplate instead.")
	void InitializeEntityTemplate(FMassEntityTemplate& InOutTemplate) const;

	FMassEntityManager& GetEntityManagerChecked() { check(EntityManager); return *EntityManager; }

protected:
	static TMap<const UScriptStruct*, FStructToTemplateBuilderDelegate> StructBasedBuilders;

	TMap<FMassEntityTemplateID, TSharedRef<FMassEntityTemplate>> TemplateIDToTemplateMap;

	/** 
	 * EntityManager the hosted templates are associated with. Storing instead of fetching at runtime to ensure all 
	 *	templates are tied to the same EntityManager
	 */
	TSharedPtr<FMassEntityManager> EntityManager;

	TWeakObjectPtr<UObject> Owner;
};


UCLASS(deprecated, meta = (DeprecationMessage = "UMassEntityTemplateRegistry is deprecated starting UE5.2. Use FMassEntityTemplateRegistry instead"))
class MASSSPAWNER_API UDEPRECATED_MassEntityTemplateRegistry : public UObject
{
	GENERATED_BODY()
};

#undef ENSURE_SUPPORTED_TRAIT_OPERATION
