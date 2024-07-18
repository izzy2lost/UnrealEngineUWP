// Copyright Epic Games, Inc. All Rights Reserved.
#include "MetasoundEditorSubsystem.h"

#include "IAssetTools.h"
#include "Kismet/KismetSystemLibrary.h"
#include "MetasoundDocumentBuilderRegistry.h"
#include "MetasoundDocumentInterface.h"
#include "MetasoundEditorGraph.h"
#include "MetasoundEditorGraphBuilder.h"
#include "MetasoundEditorGraphSchema.h"
#include "MetasoundEditorSettings.h"
#include "MetasoundFactory.h"
#include "MetasoundSettings.h"
#include "MetasoundUObjectRegistry.h"
#include "NodeTemplates/MetasoundFrontendNodeTemplateInput.h"
#include "ScopedTransaction.h"
#include "Sound/SoundSourceBusSend.h"
#include "Sound/SoundSubmixSend.h"
#include "Subsystems/AssetEditorSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetasoundEditorSubsystem)

#define LOCTEXT_NAMESPACE "MetaSoundEditor"


TScriptInterface<IMetaSoundDocumentInterface> UMetaSoundEditorSubsystem::BuildToAsset(
	UMetaSoundBuilderBase* InBuilder,
	const FString& Author,
	const FString& AssetName,
	const FString& PackagePath,
	EMetaSoundBuilderResult& OutResult,
	const USoundWave* TemplateSoundWave
)
{
	using namespace Metasound;
	using namespace Metasound::Engine;
	using namespace Metasound::Frontend;

	OutResult = EMetaSoundBuilderResult::Failed;

	if (InBuilder)
	{
		// AddToRoot to avoid builder getting gc'ed during CreateAsset call below, as the builder 
		// may be unreferenced by other UObjects and it must be persistent to finish initializing.
		const bool bWasRooted = InBuilder->IsRooted();
		if (!bWasRooted)
		{
			InBuilder->AddToRoot();
		}

		constexpr UFactory* Factory = nullptr;

		// Not about to follow this lack of const correctness down a multidecade in the works rabbit hole.
		UClass& MetaSoundUClass = const_cast<UClass&>(InBuilder->GetBaseMetaSoundUClass());
		if (UObject* NewMetaSound = IAssetTools::Get().CreateAsset(AssetName, PackagePath, &MetaSoundUClass, Factory))
		{
			InBuilder->InitNodeLocations();
			InBuilder->SetAuthor(Author);

			// Initialize and Build
			{
				constexpr UObject* Parent = nullptr;
				constexpr bool bForceUniqueClassName = true;
				constexpr bool bAddToRegistry = true;
				const FMetaSoundBuilderOptions BuilderOptions { FName(*AssetName), bForceUniqueClassName, bAddToRegistry, NewMetaSound };
				InBuilder->Build(Parent, BuilderOptions);
			}

			// Apply template SoundWave settings
			{
				const bool bIsSource = &MetaSoundUClass == UMetaSoundSource::StaticClass();
				if (InBuilder->IsPreset())
				{
					// Only use referenced UObject's SoundWave settings for sources if not overridden 
					if (TemplateSoundWave == nullptr && bIsSource)
					{
						if (const UObject* ReferencedObject = InBuilder->GetReferencedPresetAsset())
						{
							TemplateSoundWave = CastChecked<USoundWave>(ReferencedObject);
						}
					}
				}

				// Template SoundWave settings only apply to sources
				if (TemplateSoundWave != nullptr && bIsSource)
				{
					SetSoundWaveSettingsFromTemplate(*CastChecked<USoundWave>(NewMetaSound), *TemplateSoundWave);
				}
			}

			UMetaSoundBuilderBase& NewDocBuilder = FDocumentBuilderRegistry::GetChecked().FindOrBeginBuilding(*NewMetaSound);

			EMetaSoundBuilderResult InjectResult = EMetaSoundBuilderResult::Failed;
			constexpr bool bForceNodeCreation = true;
			NewDocBuilder.InjectInputTemplateNodes(bForceNodeCreation, InjectResult);

			FMetasoundAssetBase& Asset = NewDocBuilder.GetBuilder().GetMetasoundAsset();
			Asset.RebuildReferencedAssetClasses();

			if (!bWasRooted)
			{
				InBuilder->RemoveFromRoot();
			}

			OutResult = EMetaSoundBuilderResult::Succeeded;
			return NewMetaSound;
		}

		if (!bWasRooted)
		{
			InBuilder->RemoveFromRoot();
		}
	}

	return nullptr;
}

bool UMetaSoundEditorSubsystem::BindMemberMetadata(
	FMetaSoundFrontendDocumentBuilder& Builder,
	UMetasoundEditorGraphMember& InMember,
	TSubclassOf<UMetasoundEditorGraphMemberDefaultLiteral> LiteralClass,
	UMetasoundEditorGraphMemberDefaultLiteral* TemplateObject)
{
	const FGuid& MemberID = InMember.GetMemberID();

	if (TemplateObject)
	{
		Builder.ClearMemberMetadata(MemberID);
	}
	else
	{
		if (UMetaSoundFrontendMemberMetadata* Literal = Builder.FindMemberMetadata(MemberID))
		{
			InMember.Literal = CastChecked<UMetasoundEditorGraphMemberDefaultLiteral>(Literal);
			return false;
		}
	}

	if (UMetasoundEditorGraphMemberDefaultLiteral* NewLiteral = NewObject<UMetasoundEditorGraphMemberDefaultLiteral>(&Builder.CastDocumentObjectChecked<UObject>(), LiteralClass, FName(), RF_Transactional, TemplateObject))
	{
		NewLiteral->MemberID = MemberID;

		Builder.SetMemberMetadata(*NewLiteral);
		InMember.Literal = NewLiteral;
		return true;
	}

	checkNoEntry();
	return false;
}

UMetaSoundBuilderBase* UMetaSoundEditorSubsystem::FindOrBeginBuilding(TScriptInterface<IMetaSoundDocumentInterface> MetaSound, EMetaSoundBuilderResult& OutResult) const
{
	using namespace Metasound::Engine;

	if (UObject* Object = MetaSound.GetObject(); Object && Object->IsAsset())
	{
		OutResult = EMetaSoundBuilderResult::Succeeded;
		return &FDocumentBuilderRegistry::GetChecked().FindOrBeginBuilding(*Object);
	}

	OutResult = EMetaSoundBuilderResult::Failed;
	return nullptr;
}

UMetaSoundEditorSubsystem& UMetaSoundEditorSubsystem::GetChecked()
{
	checkf(GEditor, TEXT("Cannot access UMetaSoundEditorSubsystem without editor loaded"));
	UMetaSoundEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UMetaSoundEditorSubsystem>();
	checkf(EditorSubsystem, TEXT("Failed to find initialized 'UMetaSoundEditorSubsystem"));
	return *EditorSubsystem;
}

const UMetaSoundEditorSubsystem& UMetaSoundEditorSubsystem::GetConstChecked()
{
	checkf(GEditor, TEXT("Cannot access UMetaSoundEditorSubsystem without editor loaded"));
	UMetaSoundEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UMetaSoundEditorSubsystem>();
	checkf(EditorSubsystem, TEXT("Failed to find initialized 'UMetaSoundEditorSubsystem"));
	return *EditorSubsystem;
}

const FString UMetaSoundEditorSubsystem::GetDefaultAuthor()
{
	FString Author = UKismetSystemLibrary::GetPlatformUserName();
	if (const UMetasoundEditorSettings* EditorSettings = GetDefault<UMetasoundEditorSettings>())
	{
		if (!EditorSettings->DefaultAuthor.IsEmpty())
		{
			Author = EditorSettings->DefaultAuthor;
		}
	}
	return Author;
}

const TArray<TSharedRef<FExtender>>& UMetaSoundEditorSubsystem::GetToolbarExtenders() const
{
	return EditorToolbarExtenders;
}

void UMetaSoundEditorSubsystem::InitAsset(UObject& InNewMetaSound, UObject* InReferencedMetaSound)
{
	using namespace Metasound;
	using namespace Metasound::Editor;
	using namespace Metasound::Frontend;

	TScriptInterface<IMetaSoundDocumentInterface> DocInterface = &InNewMetaSound;
	FMetaSoundFrontendDocumentBuilder Builder(DocInterface);

	Builder.InitDocument();
	Builder.InitNodeLocations();

	constexpr bool bForceNodeCreation = true;
	FInputNodeTemplate::GetChecked().Inject(Builder, bForceNodeCreation);

	const FString& Author = GetDefaultAuthor();
	Builder.SetAuthor(Author);

	// Initialize asset as a preset
	if (InReferencedMetaSound)
	{
		// Ensure the referenced MetaSound is registered already
		RegisterGraphWithFrontend(*InReferencedMetaSound);

		// Initialize preset with referenced Metasound 
		TScriptInterface<IMetaSoundDocumentInterface> ReferencedDocInterface = InReferencedMetaSound;
		Builder.ConvertToPreset(ReferencedDocInterface->GetConstDocument());

		// Copy sound wave settings to preset for sources
		if (&ReferencedDocInterface->GetBaseMetaSoundUClass() == UMetaSoundSource::StaticClass())
		{
			SetSoundWaveSettingsFromTemplate(*CastChecked<USoundWave>(&InNewMetaSound), *CastChecked<USoundWave>(InReferencedMetaSound));
		}
	}
}

void UMetaSoundEditorSubsystem::InitEdGraph(UObject& InMetaSound)
{
	using namespace Metasound::Frontend;
	Metasound::Editor::FGraphBuilder::BindEditorGraph(IDocumentBuilderRegistry::GetChecked().FindOrBeginBuilding(&InMetaSound));
}

void UMetaSoundEditorSubsystem::RegisterGraphWithFrontend(UObject& InMetaSound, bool bInForceViewSynchronization) const
{
	Metasound::Editor::FGraphBuilder::RegisterGraphWithFrontend(InMetaSound, bInForceViewSynchronization);
}

void UMetaSoundEditorSubsystem::RegisterToolbarExtender(TSharedRef<FExtender> InExtender)
{
	EditorToolbarExtenders.AddUnique(InExtender);
}

void UMetaSoundEditorSubsystem::SetFocusedPage(UMetaSoundBuilderBase* Builder, FName PageName, bool bFocusPageEditor, EMetaSoundBuilderResult& OutResult) const
{
	using namespace Metasound::Frontend;
	if (!Builder)
	{
		OutResult = EMetaSoundBuilderResult::Failed;
		return;
	}

	const UMetaSoundSettings* Settings = GetDefault<UMetaSoundSettings>();
	check(Settings);
	if (const FMetaSoundPageSettings* PageSettings = Settings->FindPageSettings(PageName))
	{
		const bool bFocusedPage = SetFocusedPageInternal(*PageSettings, *Builder, bFocusPageEditor);
		if (bFocusedPage)
		{
			OutResult = EMetaSoundBuilderResult::Succeeded;
			return;
		}
	}

	OutResult = EMetaSoundBuilderResult::Failed;
}

bool UMetaSoundEditorSubsystem::SetFocusedPage(UMetaSoundBuilderBase& Builder, const FGuid& InPageID, bool bFocusPageEditor) const
{
	using namespace Metasound::Frontend;

	const UMetaSoundSettings* Settings = GetDefault<UMetaSoundSettings>();
	check(Settings);
	if (const FMetaSoundPageSettings* PageSettings = Settings->FindPageSettings(InPageID))
	{
		return SetFocusedPageInternal(*PageSettings, Builder, bFocusPageEditor);
	}

	return false;
}

bool UMetaSoundEditorSubsystem::SetFocusedPageInternal(const FMetaSoundPageSettings& InPageSettings, UMetaSoundBuilderBase& Builder, bool bFocusPageEditor) const
{
	using namespace Metasound::Frontend;

	const FScopedTransaction Transaction(FText::Format(LOCTEXT("SetFocusedPageTransactionFormat", "Set Focused Page '{0}'"), FText::FromName(InPageSettings.Name)));
	Builder.Modify();
	if (Builder.GetBuilder().SetBuildPageID(InPageSettings.UniqueId))
	{
		if (UMetasoundEditorSettings* EditorSettings = GetMutableDefault<UMetasoundEditorSettings>())
		{
			if (EditorSettings->AuditionPageMode == EAuditionPageMode::Focused)
			{
				EditorSettings->AuditionTargetPage = InPageSettings.Name;

				// Reregister to ensure all future audible instances are using the new page implementation.
				RegisterGraphWithFrontend(Builder.GetBuilder().CastDocumentObjectChecked<UObject>());
			}
		}

		if (GEditor && bFocusPageEditor)
		{
			if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
			{
				AssetEditorSubsystem->OpenEditorForAsset(&Builder.GetConstBuilder().CastDocumentObjectChecked<UObject>());
			}
		}

		return true;
	}

	return false;
}

bool UMetaSoundEditorSubsystem::UnregisterToolbarExtender(TSharedRef<FExtender> InExtender)
{
	const int32 NumRemoved = EditorToolbarExtenders.RemoveAllSwap([&InExtender](const TSharedRef<FExtender>& Extender) { return Extender == InExtender; });
	return NumRemoved > 0;
}

void UMetaSoundEditorSubsystem::SetNodeLocation(
	UMetaSoundBuilderBase* InBuilder,
	const FMetaSoundNodeHandle& InNode,
	const FVector2D& InLocation,
	EMetaSoundBuilderResult& OutResult)
{
	if (InBuilder)
	{
		InBuilder->SetNodeLocation(InNode, InLocation, OutResult);
	}
	else
	{
		OutResult = EMetaSoundBuilderResult::Failed;
	}
}

void UMetaSoundEditorSubsystem::SetSoundWaveSettingsFromTemplate(USoundWave& NewMetaSoundWave, const USoundWave& TemplateSoundWave) const
{
	// Sound 
	NewMetaSoundWave.Volume = TemplateSoundWave.Volume;
	NewMetaSoundWave.Pitch = TemplateSoundWave.Pitch;
	NewMetaSoundWave.SoundClassObject = TemplateSoundWave.SoundClassObject;

	// Attenuation 
	NewMetaSoundWave.AttenuationSettings = TemplateSoundWave.AttenuationSettings;
	NewMetaSoundWave.bDebug = TemplateSoundWave.bDebug;

	// Effects 
	NewMetaSoundWave.bEnableBusSends = TemplateSoundWave.bEnableBusSends;
	NewMetaSoundWave.SourceEffectChain = TemplateSoundWave.SourceEffectChain;
	NewMetaSoundWave.BusSends = TemplateSoundWave.BusSends;
	NewMetaSoundWave.PreEffectBusSends = TemplateSoundWave.PreEffectBusSends; 

	NewMetaSoundWave.bEnableBaseSubmix = TemplateSoundWave.bEnableBaseSubmix;
	NewMetaSoundWave.SoundSubmixObject = TemplateSoundWave.SoundSubmixObject;
	NewMetaSoundWave.bEnableSubmixSends = TemplateSoundWave.bEnableSubmixSends;
	NewMetaSoundWave.SoundSubmixSends = TemplateSoundWave.SoundSubmixSends;

	// Modulation 
	NewMetaSoundWave.ModulationSettings = TemplateSoundWave.ModulationSettings;
	
	// Voice Management 
	NewMetaSoundWave.VirtualizationMode = TemplateSoundWave.VirtualizationMode;
	NewMetaSoundWave.bOverrideConcurrency = TemplateSoundWave.bOverrideConcurrency;
	NewMetaSoundWave.ConcurrencySet = TemplateSoundWave.ConcurrencySet;
	NewMetaSoundWave.ConcurrencyOverrides = TemplateSoundWave.ConcurrencyOverrides;

	NewMetaSoundWave.bBypassVolumeScaleForPriority = TemplateSoundWave.bBypassVolumeScaleForPriority;
	NewMetaSoundWave.Priority = TemplateSoundWave.Priority;
}

#undef LOCTEXT_NAMESPACE // "MetaSoundEditor"
