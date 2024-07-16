// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Algo/Find.h"
#include "Async/Async.h"
#include "MetasoundBuilderBase.h"
#include "MetasoundDocumentInterface.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendRegistries.h"
#include "MetasoundLog.h"
#include "Misc/Optional.h"
#include "Misc/ScopeLock.h"


namespace Metasound::Engine
{
#if WITH_EDITOR
	struct FAuditionPageInfo
	{
		FName PlatformName;
		TOptional<FGuid> PageID;
	};

	DECLARE_DELEGATE_RetVal_OneParam(FAuditionPageInfo, FOnResolveAuditionPageInfo, const FMetasoundFrontendDocument& /* Document */);
#endif // WITH_EDITOR

	class METASOUNDENGINE_API FDocumentBuilderRegistry : public Frontend::IDocumentBuilderRegistry
	{
		mutable TMultiMap<FMetasoundFrontendClassName, TWeakObjectPtr<UMetaSoundBuilderBase>> Builders;

		// Critical section primarily for allowing builder collection mutation during async loading of MetaSound assets.
		mutable FCriticalSection BuildersCriticalSection;

	public:
		FDocumentBuilderRegistry() = default;
		virtual ~FDocumentBuilderRegistry();

		static FDocumentBuilderRegistry& GetChecked()
		{
			return static_cast<FDocumentBuilderRegistry&>(IDocumentBuilderRegistry::GetChecked());
		}

		enum class ELogEvent : uint8
		{
			DuplicateEntries
		};

		template <typename BuilderClass>
		BuilderClass& CreateTransientBuilder(FName BuilderName = FName())
		{
			using namespace Metasound::Frontend;

			checkf(IsInGameThread(), TEXT("Transient MetaSound builder cannot be created in non - game thread as it may result in UObject creation"));

			const EObjectFlags NewObjectFlags = RF_Public | RF_Transient;
			UPackage* TransientPackage = GetTransientPackage();
			const FName ObjectName = MakeUniqueObjectName(TransientPackage, BuilderClass::StaticClass(), BuilderName);
			TObjectPtr<BuilderClass> NewBuilder = NewObject<BuilderClass>(TransientPackage, ObjectName, NewObjectFlags);
			check(NewBuilder);
			NewBuilder->Initialize();
			const FMetasoundFrontendDocument& Document = NewBuilder->GetConstBuilder().GetConstDocumentChecked();
			const FMetasoundFrontendClassName& ClassName = Document.RootGraph.Metadata.GetClassName();
			Builders.Add(ClassName, NewBuilder);
			return *NewBuilder.Get();
		}

#if WITH_EDITORONLY_DATA
		template <typename BuilderClass = UMetaSoundBuilderBase>
		BuilderClass& FindOrBeginBuilding(UObject& InMetaSoundObject) const
		{
			check(InMetaSoundObject.IsAsset());

			TScriptInterface<IMetaSoundDocumentInterface> DocInterface = &InMetaSoundObject;
			check(DocInterface.GetObject());

			if (UMetaSoundBuilderBase* Builder = FindBuilderObject(&InMetaSoundObject))
			{
				return *CastChecked<BuilderClass>(Builder);
			}

			FNameBuilder BuilderName;
			BuilderName.Append(InMetaSoundObject.GetName());
			BuilderName.Append(TEXT("_Builder"));
			const UClass& BuilderUClass = DocInterface->GetBuilderUClass();
			const FName NewName = MakeUniqueObjectName(nullptr, &BuilderUClass, FName(*BuilderName));

			TObjectPtr<UMetaSoundBuilderBase> NewBuilder;
			{
				FScopeLock Lock(&BuildersCriticalSection);
				NewBuilder = CastChecked<UMetaSoundBuilderBase>(NewObject<UObject>(&InMetaSoundObject, &BuilderUClass, NewName, RF_Transactional));
				FMetaSoundFrontendDocumentBuilder& BuilderRef = NewBuilder->GetBuilder();
				BuilderRef = FMetaSoundFrontendDocumentBuilder(DocInterface);
				const FMetasoundFrontendDocument& Document = DocInterface->GetConstDocument();
				const FMetasoundFrontendClassName& ClassName = Document.RootGraph.Metadata.GetClassName();
				if (!ClassName.IsValid())
				{
					BuilderRef.InitDocument();
				}

				checkf(ClassName.IsValid(), TEXT("Document initialization must result in a valid class name being generated"));
				AddBuilderInternal(ClassName, NewBuilder);
			}

			return *CastChecked<BuilderClass>(NewBuilder);
		}
#endif // WITH_EDITORONLY_DATA

		// Frontend::IDocumentBuilderRegistry Implementation
#if WITH_EDITORONLY_DATA
		virtual FMetaSoundFrontendDocumentBuilder& FindOrBeginBuilding(TScriptInterface<IMetaSoundDocumentInterface> MetaSound) override;
#endif // WITH_EDITORONLY_DATA

		virtual FMetaSoundFrontendDocumentBuilder* FindBuilder(TScriptInterface<IMetaSoundDocumentInterface> MetaSound) const override;
		virtual FMetaSoundFrontendDocumentBuilder* FindBuilder(const FMetasoundFrontendClassName& InClassName, const FTopLevelAssetPath& AssetPath) const override;

		virtual FMetaSoundFrontendDocumentBuilder* FindOutermostBuilder(const UObject& InSubObject) const override;

		virtual bool FinishBuilding(const FMetasoundFrontendClassName& InClassName, bool bForceUnregisterNodeClass = false) const override;
		virtual bool FinishBuilding(const FMetasoundFrontendClassName& InClassName, const FTopLevelAssetPath& AssetPath, bool bForceUnregisterNodeClass = false) const override;

		// Returns the builder object associated with the given MetaSound asset if one is registered and active.
		UMetaSoundBuilderBase* FindBuilderObject(TScriptInterface<const IMetaSoundDocumentInterface> MetaSound) const;

		// Returns the builder object associated with the given ClassName if one is registered and active.
		// Optionally, if provided the AssetPath and there is a conflict (i.e. more than one asset is registered
		// with a given ClassName), will return the one with the provided AssetPath.  Otherwise, will arbitrarily
		// return one.
		UMetaSoundBuilderBase* FindBuilderObject(const FMetasoundFrontendClassName& InClassName, const FTopLevelAssetPath& AssetPath) const;

		// Returns all builder objects registered and active associated with the given ClassName.
		TArray<UMetaSoundBuilderBase*> FindBuilderObjects(const FMetasoundFrontendClassName& InClassName) const;

#if WITH_EDITOR
		FOnResolveAuditionPageInfo& GetOnResolveAuditionPageInfoDelegate();
#endif // WITH_EDITOR

		bool ReloadBuilder(const FMetasoundFrontendClassName& InClassName) const override;

		// Given the provided document and its respective pages, returns the PageID to be used for runtime IGraph and proxy generation.
		virtual FGuid ResolveTargetPageID(const FMetasoundFrontendDocument& Document, const FTopLevelAssetPath& AssetPath) const override;

		void SetEventLogVerbosity(ELogEvent Event, ELogVerbosity::Type Verbosity);

	private:
		void AddBuilderInternal(const FMetasoundFrontendClassName& InClassName, UMetaSoundBuilderBase* NewBuilder) const;
		bool CanPostEventLog(ELogEvent Event, ELogVerbosity::Type Verbosity) const;
		void FinishBuildingInternal(UMetaSoundBuilderBase& Builder, bool bForceUnregisterNodeClass) const;

#if WITH_EDITOR
		FOnResolveAuditionPageInfo OnResolveAuditionPageInfo;
#endif // WITH_EDITOR

		TSortedMap<ELogEvent, ELogVerbosity::Type> EventLogVerbosity;
	};
} // namespace Metasound::Engine