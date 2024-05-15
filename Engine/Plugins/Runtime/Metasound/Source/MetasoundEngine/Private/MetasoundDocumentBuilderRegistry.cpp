// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundDocumentBuilderRegistry.h"

#include "MetasoundTrace.h"
#include "MetasoundUObjectRegistry.h"


namespace Metasound::Engine
{
	FDocumentBuilderRegistry::~FDocumentBuilderRegistry()
	{
#if !NO_LOGGING
		if (!Builders.IsEmpty())
		{
			TArray<TPair<FMetasoundFrontendClassName, TWeakObjectPtr<UMetaSoundBuilderBase>>> PairsToFinish;
			int32 NumStale = 0;
			for (const TPair<FMetasoundFrontendClassName, TWeakObjectPtr<UMetaSoundBuilderBase>>& Pair : Builders)
			{
				if (Pair.Value.IsValid())
				{
					PairsToFinish.Add(Pair);
				}
				else
				{
					NumStale++;
				}
			}

			UE_LOG(LogMetaSound, Warning, TEXT("BuilderRegistry is shutting down with existing records: %i stale entries never removed, the following %i are still active and will be forcefully shutdown:"), NumStale, PairsToFinish.Num());
			for (const TPair<FMetasoundFrontendClassName, TWeakObjectPtr<UMetaSoundBuilderBase>>& Pair : PairsToFinish)
			{
				constexpr bool bForceUnregister = true;
				FinishBuilding(Pair.Key, bForceUnregister);
				UE_LOG(LogMetaSound, Warning, TEXT("- %s"), *Pair.Value->GetFullName());
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
		if (UObject* Object = MetaSound.GetObject())
		{
			return FindBuilder(MetaSound->GetConstDocument().RootGraph.Metadata.GetClassName());
		}

		return nullptr;
	}

	FMetaSoundFrontendDocumentBuilder* FDocumentBuilderRegistry::FindBuilder(const FMetasoundFrontendClassName& InClassName) const
	{
		using namespace Metasound::Engine;
		if (UMetaSoundBuilderBase* Builder = Builders.FindRef(InClassName).Get())
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
			return Builders.FindRef(ClassName).Get();
		}

		return nullptr;
	}

	UMetaSoundBuilderBase* FDocumentBuilderRegistry::FindBuilderObject(const FMetasoundFrontendClassName& ClassName) const
	{
		return Builders.FindRef(ClassName).Get();
	}

	bool FDocumentBuilderRegistry::FinishBuilding(const FMetasoundFrontendClassName& InClassName, bool bForceUnregister) const
	{
		using namespace Metasound;
		using namespace Metasound::Engine;

		if (TWeakObjectPtr<UMetaSoundBuilderBase>* BuilderPtr = Builders.Find(InClassName))
		{
			if (UMetaSoundBuilderBase* Builder = BuilderPtr->Get())
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
						if (bForceUnregister || LastTransactionRegistered != TransactionCount)
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

			// Still return true in this case as the builder likely has become inaccessible and may be in the "beginning" of destruction,
			// so entries is still reporting that it was successfully removed.
			ensureAlways(Builders.Remove(InClassName));
			return true;
		}

		return false;
	}
} // namespace Metasound::Engine