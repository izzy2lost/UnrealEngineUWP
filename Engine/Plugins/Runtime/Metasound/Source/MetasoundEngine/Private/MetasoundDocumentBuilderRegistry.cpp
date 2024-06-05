// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundDocumentBuilderRegistry.h"

#include "MetasoundTrace.h"
#include "MetasoundUObjectRegistry.h"


namespace Metasound::Engine
{
	FDocumentBuilderRegistry::~FDocumentBuilderRegistry()
	{
		TMultiMap<FMetasoundFrontendClassName, TWeakObjectPtr<UMetaSoundBuilderBase>> BuildersToFinish;
		FScopeLock Lock(&BuildersCriticalSection);
		{
			BuildersToFinish = MoveTemp(Builders);
			Builders.Reset();
		}

		UE_CLOG(!BuildersToFinish.IsEmpty(), LogMetaSound, Display, TEXT("BuilderRegistry is shutting down with the following %i active builder entries. Forcefully shutting down:"), BuildersToFinish.Num());
		int32 NumStale = 0;
		for (const TPair<FMetasoundFrontendClassName, TWeakObjectPtr<UMetaSoundBuilderBase>>& Pair : BuildersToFinish)
		{
			const bool bIsValid = Pair.Value.IsValid();
			if (bIsValid)
			{
				UE_CLOG(bIsValid, LogMetaSound, Display, TEXT("- %s"), *Pair.Value->GetFullName());
				constexpr bool bForceUnregister = true;
				FinishBuildingInternal(Pair.Key, bForceUnregister);
			}
			else
			{
				++NumStale;
			}
		}
		UE_CLOG(NumStale > 0, LogMetaSound, Display, TEXT("BuilderRegistry is shutting down with %i stale entries"), NumStale);
	}

	void FDocumentBuilderRegistry::AddBuilderInternal(const FMetasoundFrontendClassName& InClassName, UMetaSoundBuilderBase* NewBuilder) const
	{
		FScopeLock Lock(&BuildersCriticalSection);

#if !NO_LOGGING
		const bool bPrintConflicts = Builders.Contains(InClassName);
#endif // !NO_LOGGING

		Builders.Add(InClassName, NewBuilder);

#if !NO_LOGGING
		if (bPrintConflicts)
		{
			TArray<TWeakObjectPtr<UMetaSoundBuilderBase>> Entries;
			Builders.MultiFind(InClassName, Entries);
			UE_LOG(LogMetaSound, Error, TEXT("More than one asset registered with class name '%s'. "
				"Returning builder that may not be associated with desired object! \n"
				"This can happen if asset was moved using revision control and original location was revived. \n"
				"Remove all but one of the following assets and relink a duplicate or copied replacement asset:"),
				*InClassName.ToString());
			for (const TWeakObjectPtr<UMetaSoundBuilderBase>& BuilderPtr : Entries)
			{
				if (BuilderPtr.IsValid())
				{
					UE_LOG(LogMetaSound, Error, TEXT("- %s"), *BuilderPtr->GetConstBuilder().CastDocumentObjectChecked<UObject>().GetPathName());
				}
				else
				{
					UE_LOG(LogMetaSound, Error, TEXT("- STALE ENTRY (Not removed prior to asset removal)"));
				}
			}
		}
#endif // !NO_LOGGING
	}

#if WITH_EDITORONLY_DATA
	FMetaSoundFrontendDocumentBuilder& FDocumentBuilderRegistry::FindOrBeginBuilding(TScriptInterface<IMetaSoundDocumentInterface> MetaSound)
	{
		UObject* Object = MetaSound.GetObject();
		check(Object);

		return FindOrBeginBuilding(*Object).GetBuilder();
	}
#endif // WITH_EDITORONLY_DATA

	FMetaSoundFrontendDocumentBuilder* FDocumentBuilderRegistry::FindBuilder(TScriptInterface<IMetaSoundDocumentInterface> MetaSound) const
	{
		if (UMetaSoundBuilderBase* Builder = FindBuilderObject(MetaSound))
		{
			return &Builder->GetBuilder();
		}

		return nullptr;
	}

	FMetaSoundFrontendDocumentBuilder* FDocumentBuilderRegistry::FindBuilder(const FMetasoundFrontendClassName& InClassName) const
	{
		if (UMetaSoundBuilderBase* Builder = FindBuilderObject(InClassName))
		{
			return &Builder->GetBuilder();
		}

		return nullptr;
	}

	UMetaSoundBuilderBase* FDocumentBuilderRegistry::FindBuilderObject(TScriptInterface<const IMetaSoundDocumentInterface> MetaSound) const
	{
		if (const UObject* MetaSoundObject = MetaSound.GetObject())
		{
			const FMetasoundFrontendDocument& Document = MetaSound->GetConstDocument();
			const FMetasoundFrontendClassName& ClassName = Document.RootGraph.Metadata.GetClassName();
			TArray<TWeakObjectPtr<UMetaSoundBuilderBase>> Entries;

			{
				FScopeLock Lock(&BuildersCriticalSection);
				Builders.MultiFind(ClassName, Entries);
			}

			if (!Entries.IsEmpty())
			{
				auto IsMetaSound = [MetaSoundObject = MetaSound.GetObject()](const TWeakObjectPtr<UMetaSoundBuilderBase>& BuilderPtr)
				{
					if (BuilderPtr.IsValid())
					{
						UObject& TestMetaSound = BuilderPtr->GetConstBuilder().CastDocumentObjectChecked<UObject>();
						return &TestMetaSound == MetaSoundObject;
					}
					return false;
				};
				const TWeakObjectPtr<UMetaSoundBuilderBase>* Builder = Entries.FindByPredicate(IsMetaSound);
				if (Builder)
				{
					return Builder->Get();
				}
			}
		}

		return nullptr;
	}

	UMetaSoundBuilderBase* FDocumentBuilderRegistry::FindBuilderObject(const FMetasoundFrontendClassName& InClassName) const
	{
		TArray<TWeakObjectPtr<UMetaSoundBuilderBase>> Entries;
		{
			FScopeLock Lock(&BuildersCriticalSection);
			Builders.MultiFind(InClassName, Entries);
		}
		if (!Entries.IsEmpty())
		{
			return Entries.Last().Get();
		}

		return nullptr;
	}

	TArray<UMetaSoundBuilderBase*> FDocumentBuilderRegistry::FindBuilderObjects(const FMetasoundFrontendClassName& InClassName) const
	{
		TArray<UMetaSoundBuilderBase*> FoundBuilders;
		TArray<TWeakObjectPtr<UMetaSoundBuilderBase>> Entries;

		{
			FScopeLock Lock(&BuildersCriticalSection);
			Builders.MultiFind(InClassName, Entries);
		}

		if (!Entries.IsEmpty())
		{
			Algo::TransformIf(Entries, FoundBuilders,
				[](const TWeakObjectPtr<UMetaSoundBuilderBase>& BuilderPtr) { return BuilderPtr.IsValid(); },
				[](const TWeakObjectPtr<UMetaSoundBuilderBase>& BuilderPtr) { return BuilderPtr.Get(); }
			);
		}

		return FoundBuilders;
	}

	FMetaSoundFrontendDocumentBuilder* FDocumentBuilderRegistry::FindOutermostBuilder(const UObject& InSubObject) const
	{
		using namespace Metasound::Frontend;
		TScriptInterface<IMetaSoundDocumentInterface> DocumentInterface = InSubObject.GetOutermostObject();
		check(DocumentInterface.GetObject());
		return FindBuilder(DocumentInterface);
	}

	bool FDocumentBuilderRegistry::FinishBuilding(const FMetasoundFrontendClassName& InClassName, bool bForceUnregisterNodeClass) const
	{
		using namespace Metasound;
		using namespace Metasound::Engine;

		FinishBuildingInternal(InClassName, bForceUnregisterNodeClass);

		// Still return true in this case as the builder likely has become inaccessible and may be in the "beginning" of destruction,
		// so entries is still reporting that it was successfully removed.
		{
			FScopeLock Lock(&BuildersCriticalSection);
			return Builders.Remove(InClassName) > 0;
		}
	}

	void FDocumentBuilderRegistry::FinishBuildingInternal(const FMetasoundFrontendClassName& InClassName, bool bForceUnregisterNodeClass) const
	{
		TArray<UMetaSoundBuilderBase*> FoundBuilders = FindBuilderObjects(InClassName);
		for (UMetaSoundBuilderBase* Builder : FoundBuilders)
		{
			// If the builder has applied transactions to its document object that are not mirrored in the frontend registry,
			// unregister version in registry. This will ensure that future requests for the builder's associated asset will
			// register a fresh version from the object as the transaction history is intrinsically lost once this builder
			// is destroyed. It is also possible that the DocBuilder's underlying object can be invalid if object was force
			// deleted, so validity check is necessary.
			FMetaSoundFrontendDocumentBuilder& DocBuilder = Builder->GetBuilder();
			if (DocBuilder.IsValid())
			{
				if (!IsRunningCookCommandlet())
				{
					const int32 TransactionCount = DocBuilder.GetTransactionCount();
					const int32 LastTransactionRegistered = Builder->GetLastTransactionRegistered();
					if (bForceUnregisterNodeClass || LastTransactionRegistered != TransactionCount)
					{
						UObject& MetaSound = DocBuilder.CastDocumentObjectChecked<UObject>();
						if (FMetasoundAssetBase* MetaSoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(&MetaSound))
						{
							MetaSoundAsset->UnregisterGraphWithFrontend();
						}
					}
				}
				DocBuilder.FinishBuilding();
			}
		}
	}

	bool FDocumentBuilderRegistry::ReloadBuilder(const FMetasoundFrontendClassName& InClassName) const
	{
		if (UMetaSoundBuilderBase* Builder = FindBuilderObject(InClassName))
		{
			Builder->Reload();
			return true;
		}

		return false;
	}
} // namespace Metasound::Engine