// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/UnrealString.h"
#include "MetasoundDocumentInterface.h"
#include "MetasoundFrontendController.h"
#include "MetasoundFrontendTransform.h"
#include "MetasoundFrontendDocument.h"
#include "UObject/NameTypes.h"
#include "UObject/ScriptInterface.h"

class IMetaSoundDocumentInterface;


namespace Metasound::Frontend
{
	/** Base class for versioning a document. */
	class METASOUNDFRONTEND_API FVersionDocument
	{
		const FName Name;
		const FString& Path;

	public:
		static FMetasoundFrontendVersionNumber GetMaxVersion()
		{
			return FMetasoundFrontendVersionNumber { 1, 12 };
		}

		FVersionDocument(FName InName, const FString& InPath);

		UE_DEPRECATED(5.4, "Use DocumentInterface overload instead")
		bool Transform(FDocumentHandle InDocument) const;

		bool Transform(TScriptInterface<IMetaSoundDocumentInterface> DocumentInterface) const;
	};
} // namespace Metasound::Frontend
