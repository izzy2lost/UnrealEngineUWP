// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundDocumentInterface.h"


namespace Metasound::Frontend
{
	namespace DocumentInterfacePrivate
	{
		TUniquePtr<IDocumentBuilderRegistry> Instance;
	}

	IDocumentBuilderRegistry* IDocumentBuilderRegistry::Get()
	{
		return DocumentInterfacePrivate::Instance.Get();
	}

	IDocumentBuilderRegistry& IDocumentBuilderRegistry::GetChecked()
	{
		return *DocumentInterfacePrivate::Instance.Get();
	}

	void IDocumentBuilderRegistry::Deinitialize()
	{
		check(DocumentInterfacePrivate::Instance.IsValid());
		DocumentInterfacePrivate::Instance.Reset();
	}

	void IDocumentBuilderRegistry::Initialize(TUniquePtr<IDocumentBuilderRegistry>&& InInstance)
	{
		check(!DocumentInterfacePrivate::Instance.IsValid());
		DocumentInterfacePrivate::Instance = MoveTemp(InInstance);
	}

	IMetaSoundDocumentBuilderRegistry& IMetaSoundDocumentBuilderRegistry::GetChecked()
	{
		return static_cast<IMetaSoundDocumentBuilderRegistry&>(IDocumentBuilderRegistry::GetChecked());
	}
} // namespace Metasound::Frontend