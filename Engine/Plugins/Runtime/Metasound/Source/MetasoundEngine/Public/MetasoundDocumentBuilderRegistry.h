// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Algo/Find.h"
#include "MetasoundBuilderBase.h"
#include "MetasoundDocumentInterface.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendRegistries.h"
#include "MetasoundLog.h"


namespace Metasound::Engine
{
	class METASOUNDENGINE_API FDocumentBuilderRegistry : public Frontend::IDocumentBuilderRegistry
	{
		mutable TMap<FMetasoundFrontendClassName, TWeakObjectPtr<UMetaSoundBuilderBase>> Builders;

	public:
		FDocumentBuilderRegistry() = default;
		virtual ~FDocumentBuilderRegistry();

		static FDocumentBuilderRegistry& GetChecked()
		{
			return static_cast<FDocumentBuilderRegistry&>(IDocumentBuilderRegistry::GetChecked());
		}

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
			const FMetasoundFrontendDocument& Document = NewBuilder->GetConstBuilder().GetConstDocument();
			const FMetasoundFrontendClassName& ClassName = Document.RootGraph.Metadata.GetClassName();
			Builders.Add(ClassName, NewBuilder);
			return *NewBuilder.Get();
		}

#if WITH_EDITORONLY_DATA
		template <typename BuilderClass = UMetaSoundBuilderBase>
		BuilderClass& FindOrBeginBuilding(UObject& InMetaSoundObject) const
		{
			check(InMetaSoundObject.IsAsset());
			checkf(IsInGameThread(), TEXT("Asset MetaSound Builder cannot be created in non-game thread as it may result in UObject creation"));

			TScriptInterface<IMetaSoundDocumentInterface> DocInterface = &InMetaSoundObject;
			check(DocInterface.GetObject());

			const FMetasoundFrontendDocument& Document = DocInterface->GetConstDocument();
			const FMetasoundFrontendClassName& FullClassName = Document.RootGraph.Metadata.GetClassName();

			if (FullClassName.IsValid())
			{
				TWeakObjectPtr<UMetaSoundBuilderBase> Builder = Builders.FindRef(FullClassName);
				if (Builder.IsValid())
				{
					return *CastChecked<BuilderClass>(Builder.Get());
				}
			}

			TObjectPtr<UMetaSoundBuilderBase> NewBuilder = CastChecked<UMetaSoundBuilderBase>(NewObject<UObject>(&InMetaSoundObject, &DocInterface->GetBuilderUClass()));
			FMetaSoundFrontendDocumentBuilder& BuilderRef = NewBuilder->GetBuilder();
			BuilderRef = FMetaSoundFrontendDocumentBuilder(DocInterface);

			if (!FullClassName.IsValid())
			{
				BuilderRef.InitDocument();
			}

			checkf(FullClassName.IsValid(), TEXT("Document initialization must result in a valid class name being generated"));
			TObjectPtr<UMetaSoundBuilderBase> NewBuilderBase = CastChecked<UMetaSoundBuilderBase>(NewBuilder);
			Builders.Add(FullClassName, NewBuilderBase);
			return *CastChecked<BuilderClass>(NewBuilder);
		}
#endif // WITH_EDITORONLY_DATA

		// Frontend::IDocumentBuilderRegistry Implementation
#if WITH_EDITORONLY_DATA
		virtual FMetaSoundFrontendDocumentBuilder& FindOrBeginBuilding(TScriptInterface<IMetaSoundDocumentInterface> MetaSound) override;
#endif // WITH_EDITORONLY_DATA

		virtual FMetaSoundFrontendDocumentBuilder* FindBuilder(TScriptInterface<IMetaSoundDocumentInterface> MetaSound) const override;
		virtual FMetaSoundFrontendDocumentBuilder* FindBuilder(const FMetasoundFrontendClassName& InClassName) const override;
		virtual bool FinishBuilding(const FMetasoundFrontendClassName& InClassName) const override;

		UMetaSoundBuilderBase* FindBuilderObject(TScriptInterface<const IMetaSoundDocumentInterface> MetaSound) const;
		UMetaSoundBuilderBase* FindBuilderObject(const FMetasoundFrontendClassName& ClassName) const;
	};
} // namespace Metasound::Engine