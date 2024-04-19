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
	static FMetasoundFrontendVersionNumber GetMaxDocumentVersion()
	{
		return FMetasoundFrontendVersionNumber { 1, 12 };
	}

	// Versions Frontend Document. Passed as AssetBase for backward compat to
	// version asset documents predating the IMetaSoundDocumentInterface
	bool VersionDocument(FMetasoundAssetBase& InAssetBase);
} // namespace Metasound::Frontend
