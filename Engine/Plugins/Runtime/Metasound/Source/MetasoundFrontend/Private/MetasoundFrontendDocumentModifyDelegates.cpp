// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundFrontendDocumentModifyDelegates.h"


namespace Metasound::Frontend
{
	FDocumentModifyDelegates::FDocumentModifyDelegates()
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		: NodeDelegates()
		, EdgeDelegates()
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	{
	}

	FDocumentModifyDelegates::FDocumentModifyDelegates(const FMetasoundFrontendDocument& Document)
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		: NodeDelegates()
		, EdgeDelegates()
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	{
		Document.RootGraph.IterateGraphPages([this](const FMetasoundFrontendGraph& Graph)
		{
			AddPageDelegates(Graph.PageID);
		});
	}

	void FDocumentModifyDelegates::AddPageDelegates(const FGuid& InPageID)
	{
		PageNodeDelegates.Add(InPageID, FNodeModifyDelegates());
		PageEdgeDelegates.Add(InPageID, FEdgeModifyDelegates());

		PageDelegates.OnPageAdded.Broadcast(FDocumentMutatePageArgs{ InPageID });
	}

	void FDocumentModifyDelegates::RemovePageDelegates(const FGuid& InPageID)
	{
		PageDelegates.OnRemovingPage.Broadcast(FDocumentMutatePageArgs { InPageID });

		PageNodeDelegates.Remove(InPageID);
		PageEdgeDelegates.Remove(InPageID);
	}
} // namespace Metasound::Frontend
