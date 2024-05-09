// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundDocumentBuilderRegistry.h"

#include "MetasoundTrace.h"
#include "MetasoundUObjectRegistry.h"


namespace Metasound::Engine
{
	FDocumentBuilderRegistry::~FDocumentBuilderRegistry()
	{
		for(const TPair<FMetasoundFrontendClassName, TWeakObjectPtr<UMetaSoundBuilderBase>>& Pair : Builders)
		{
			TWeakObjectPtr<UMetaSoundBuilderBase> Builder = Pair.Value;
			if (Builder.IsValid())
			{
				// If the builder has applied transactions to its document object that are not mirrored in the frontend registry,
				// unregister version in registry. This will ensure that future requests for the builder's associated asset will
				// register a fresh version from the object as the transaction history is intrinsically lost once this builder
				// is destroyed.
				if (Builder->GetLastTransactionRegistered() != Builder->GetConstBuilder().GetTransactionCount())
				{
					UObject& MetaSound = Builder->GetConstBuilder().CastDocumentObjectChecked<UObject>();
					if (FMetasoundAssetBase* MetaSoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(&MetaSound))
					{
						MetaSoundAsset->UnregisterGraphWithFrontend();
					}
				}
			}
		}
		Builders.Reset();
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

		TWeakObjectPtr<UMetaSoundBuilderBase> Builder = Builders.FindRef(InClassName);
		if (Builder.IsValid())
		{
			// If the builder has applied transactions to its document object that are not mirrored in the frontend registry,
			// unregister version in registry. This will ensure that future requests for the builder's associated asset will
			// register a fresh version from the object as the transaction history is intrinsically lost once this builder
			// is destroyed. It is also possible that the DocBuilder's underlying object can be invalid if object was force
			// deleted, so validity check is necessary.
			FMetaSoundFrontendDocumentBuilder& DocBuilder = Builder->GetBuilder();
			if (DocBuilder.IsValid())
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
			ensureAlways(Builders.Remove(InClassName));
			return true;
		}

		return false;
	}
} // namespace Metasound::Engine