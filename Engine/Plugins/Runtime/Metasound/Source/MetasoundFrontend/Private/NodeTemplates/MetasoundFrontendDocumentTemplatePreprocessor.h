// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendTransform.h"


namespace Metasound
{
	namespace Frontend
	{
		class FDocumentTemplatePreprocessTransform : public IDocumentTransform
		{
		public:
			UE_DEPRECATED(5.4, "TemplatePreprocessTransform is now FMetasoundFrontendDocumentBuilder::TransformTemplateNodes")
			virtual bool Transform(FMetasoundFrontendDocument& InOutDocument) const override;
		};

		// Base implementation for preprocessing a given template node
		class FNodeTemplatePreprocessTransformBase : public INodeTransform
		{
		protected:
			FMetasoundFrontendDocument& Document;
			FMetasoundFrontendGraph& Graph;

		public:
			FNodeTemplatePreprocessTransformBase(FMetasoundFrontendDocument& InDocument)
				: Document(InDocument)
				, Graph(InDocument.RootGraph.Graph)
			{
			}

			virtual ~FNodeTemplatePreprocessTransformBase() = default;
		};

	} // namespace Frontend
} // namespace Metasound
