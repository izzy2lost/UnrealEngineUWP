// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassEntityTemplateRegistry.h"
#include "MassSpawnerTypes.h"
#include "MassEntityManager.h"
#include "Engine/World.h"
#include "VisualLogger/VisualLogger.h"
#include "HAL/IConsoleManager.h"
#include "MassSpawnerSubsystem.h"
#include "MassEntityTypes.h"
#include "MassEntityTraitBase.h"
#include "Logging/MessageLog.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Misc/UObjectToken.h"
#include "Framework/Docking/TabManager.h"
#endif

#define LOCTEXT_NAMESPACE "Mass"

//----------------------------------------------------------------------//
// FMassEntityTemplateRegistry 
//----------------------------------------------------------------------//
TMap<const UScriptStruct*, FMassEntityTemplateRegistry::FStructToTemplateBuilderDelegate> FMassEntityTemplateRegistry::StructBasedBuilders;

FMassEntityTemplateRegistry::FMassEntityTemplateRegistry(UObject* InOwner)
	: Owner(InOwner)
{
}

void FMassEntityTemplateRegistry::ShutDown()
{
	TemplateIDToTemplateMap.Reset();
	EntityManager = nullptr;
}

UWorld* FMassEntityTemplateRegistry::GetWorld() const 
{
	return Owner.IsValid() ? Owner->GetWorld() : nullptr;
}

FMassEntityTemplateRegistry::FStructToTemplateBuilderDelegate& FMassEntityTemplateRegistry::FindOrAdd(const UScriptStruct& DataType)
{
	return StructBasedBuilders.FindOrAdd(&DataType);
}

void FMassEntityTemplateRegistry::Initialize(const TSharedPtr<FMassEntityManager>& InEntityManager)
{
	if (EntityManager)
	{
		ensureMsgf(EntityManager == InEntityManager, TEXT("Attempting to store a different EntityManager then the previously stored one - this indicated a set up issue, attempting to use multiple EntityManager instances"));
		return;
	}

	EntityManager = InEntityManager;
}

void FMassEntityTemplateRegistry::DebugReset()
{
#if WITH_MASSGAMEPLAY_DEBUG
	TemplateIDToTemplateMap.Reset();
#endif // WITH_MASSGAMEPLAY_DEBUG
}

const TSharedRef<FMassEntityTemplate>* FMassEntityTemplateRegistry::FindTemplateFromTemplateID(FMassEntityTemplateID TemplateID) const
{
	return TemplateIDToTemplateMap.Find(TemplateID);
}

const TSharedRef<FMassEntityTemplate>& FMassEntityTemplateRegistry::FindOrAddTemplate(FMassEntityTemplateID TemplateID, FMassEntityTemplateData&& TemplateData)
{
	check(EntityManager);
	const TSharedRef<FMassEntityTemplate>* ExistingTemplate = FindTemplateFromTemplateID(TemplateID);
	if (ExistingTemplate != nullptr)
	{
		return *ExistingTemplate;
	}

	return TemplateIDToTemplateMap.Add(TemplateID, FMassEntityTemplate::MakeFinalTemplate(*EntityManager, MoveTemp(TemplateData), TemplateID));
}

void FMassEntityTemplateRegistry::DestroyTemplate(FMassEntityTemplateID TemplateID)
{
	TemplateIDToTemplateMap.Remove(TemplateID);
}

//----------------------------------------------------------------------//
// FMassEntityTemplateBuildContext 
//----------------------------------------------------------------------//
bool FMassEntityTemplateBuildContext::BuildFromTraits(TConstArrayView<UMassEntityTraitBase*> Traits, const UWorld& World)
{
	ensureMsgf(bBuildInProgress == false, TEXT("Unexpected occurrence - it suggests FMassEntityTemplateBuildContext::BuildFromTraits "
		"has been called as a consequence of some UMassEntityTraitBase::BuildTemplate call. Check the callstack."));

	bBuildInProgress = true;
	for (const UMassEntityTraitBase* Trait : Traits)
	{
		check(Trait);
		if (SetTraitBeingProcessed(Trait))
		{
			Trait->BuildTemplate(*this, World);
		}
	}
	bBuildInProgress = false;

	const bool bTemplateValid = ValidateBuildContext(World);
	
	ResetBuildTimeData();

	return bTemplateValid;
}

bool FMassEntityTemplateBuildContext::SetTraitBeingProcessed(const UMassEntityTraitBase* Trait)
{
	if (Trait == nullptr || TraitsProcessed.Contains(Trait) == false)
	{
		TraitsData.Add({Trait});
		return true;
	}

	UE_LOG(LogMass, Warning, TEXT("Attempting to add %s to FMassEntityTemplateBuildContext while this or another instance of the trait class has already been added.")
		, *GetNameSafe(Trait));

	IgnoredTraits.Add(Trait);
	return false;
}

bool FMassEntityTemplateBuildContext::ValidateBuildContext(const UWorld& World)
{
#define WITH_MESSAGES (WITH_UNREAL_DEVELOPER_TOOLS && WITH_EDITOR)
#if WITH_MESSAGES
	TArray<TSharedRef<FTokenizedMessage>> Messages;
#define IF_MESSAGES(Message) if (GEditor) { Message }
#else
#define IF_MESSAGES(_)
#endif // WITH_MESSAGES

	int32 ErrorCount = 0;
	int32 WarningCount = 0;


	TMap<const UStruct*, const UMassEntityTraitBase*> TypesAlreadyAdded;

	// these are non-critical warnings, we want to report these to the users as a potential configuration issue,
	// but it won't affect the final entity template composition (for example adding the same fragment is fine since 
	// the entity template handles that gracefully).
	for (const FTraitData& TraitData : TraitsData)
	{
		for (const UStruct* TypeAdded : TraitData.TypesAdded)
		{
			const UMassEntityTraitBase*& SourceTrait = TypesAlreadyAdded.FindOrAdd(TypeAdded);
			if (SourceTrait != nullptr)
			{
				// we report this only if it wasn't added twice by the same trait, the one we're processing right now
				UE_CLOG(SourceTrait != TraitData.Trait
					, LogMass, Warning, TEXT("%s: Fragment %s already added by %s")
					, *GetNameSafe(TraitData.Trait), *GetNameSafe(TypeAdded), *SourceTrait->GetName());
				++WarningCount;

				IF_MESSAGES(
					Messages.Add_GetRef(FTokenizedMessage::Create(EMessageSeverity::Warning))
						->AddToken(FUObjectToken::Create(TraitData.Trait))
						->AddToken(FTextToken::Create(LOCTEXT("MassEntityTraitFragmentDuplicationWarning1", "trying to add fragment of type")))
						->AddToken(FUObjectToken::Create(TypeAdded))
						->AddToken(FTextToken::Create(LOCTEXT("MassEntityTraitFragmentDuplicationWarning2", "while it has already been added by")))
						->AddToken(FUObjectToken::Create(SourceTrait));
				);
			}
			else
			{
				SourceTrait = TraitData.Trait;
			}
		}
	}

	// these are critical, we're going to fail the validation if anything here fails
	for (const FTraitData& TraitData : TraitsData)
	{
		for (const UStruct* TypeRequired : TraitData.TypesRequired)
		{
			if (TypesAlreadyAdded.Contains(TypeRequired) == false)
			{
				UE_LOG(LogMass, Error, TEXT("%s: Missing required fragment of type %s")
					, *GetNameSafe(TraitData.Trait), *GetNameSafe(TypeRequired));
				++ErrorCount;
				IF_MESSAGES(
					Messages.Add_GetRef(FTokenizedMessage::Create(EMessageSeverity::Error))
						->AddToken(FUObjectToken::Create(TraitData.Trait))
						->AddToken(FTextToken::Create(LOCTEXT("MassEntityTraitMissingDependencies", "unsatisfied dependency, missing")))
						->AddToken(FUObjectToken::Create(TypeRequired));
				);
			}
		}
	}

	for (const FTraitData& TraitData : TraitsData)
	{
		if (TraitData.Trait && TraitData.Trait->ValidateTemplate(*this, World) == false)
		{
			++ErrorCount;
			IF_MESSAGES(
				Messages.Add_GetRef(FTokenizedMessage::Create(EMessageSeverity::Error))
					->AddToken(FUObjectToken::Create(TraitData.Trait))
					->AddToken(FTextToken::Create(LOCTEXT("MassEntityTraitFailedValidation", "trait-specific validation failed")));
			);
		}
	}

	for (const UMassEntityTraitBase* IgnoredTrait : IgnoredTraits)
	{
		IF_MESSAGES(
			Messages.Add_GetRef(FTokenizedMessage::Create(EMessageSeverity::Warning))
				->AddToken(FUObjectToken::Create(IgnoredTrait))
				->AddToken(FTextToken::Create(LOCTEXT("MassEntityTraitIgnoredTrait", "trait was ignored. Check if it's not a duplicate.")));
		);
		++WarningCount;
	}
	
	// @todo add dependencies on trait classes? might be hard if traits are unrelated, like requiring UMassLODCollectorTrait 
	// or UMassDistanceLODCollectorTrait - both supply alternative implementations of a given functionality, but are unrelated.
	// Could be done with a complex requirements system (similar to entity queries - "all of X", "any of Y", etc) - probably 
	// not worth it since we don't even have a use case for it right now.

#if WITH_MESSAGES
	if (GEditor && Messages.Num())
	{
		TSharedRef<FTokenizedMessage> SummaryMessage = Messages.Add_GetRef(
			FTokenizedMessage::Create(ErrorCount ? EMessageSeverity::Error : EMessageSeverity::Warning))
			->AddToken(FTextToken::Create(FText::FormatOrdered(LOCTEXT("MassEntityTraitResult", "Mass Entity Template validation:\n{0} errors and {1} warnings found"), ErrorCount, WarningCount)));

		Messages.Add_GetRef(FTokenizedMessage::Create(EMessageSeverity::Info))
			->AddToken(FActionToken::Create(LOCTEXT("MassSeeLogForDetails", "See the log for more details.")
				, LOCTEXT("MassSeeLogForDetailsTooltip", "Open the Output Log tab.")
				, FOnActionTokenExecuted::CreateLambda([]()
					{
						FGlobalTabmanager::Get()->TryInvokeTab(FName("OutputLog"));
					}))
				);

		FMessageLog EditorErrors("MassEntity");
		EditorErrors.AddMessages(Messages);
		EditorErrors.Notify(SummaryMessage->ToText());
	}
#endif // WITH_MESSAGES

#undef IF_MESSAGES
#undef WITH_MESSAGES

	// only the Errors render the template invalid, Warnings just warn about stuff not being set up quite right, but we can recover.
	return (ErrorCount == 0);
}

//-----------------------------------------------------------------------------
// DEPRECATED
//-----------------------------------------------------------------------------
FMassEntityTemplate* FMassEntityTemplateRegistry::FindMutableTemplateFromTemplateID(FMassEntityTemplateID TemplateID)
{
	return nullptr;
}

FMassEntityTemplate& FMassEntityTemplateRegistry::CreateTemplate(const uint32 HashLookup, FMassEntityTemplateID TemplateID)
{
	static FMassEntityTemplate Dummy;
	return Dummy;
}

void FMassEntityTemplateRegistry::InitializeEntityTemplate(FMassEntityTemplate& InOutTemplate) const
{}

#undef LOCTEXT_NAMESPACE 
