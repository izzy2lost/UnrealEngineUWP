// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "MLDeformerMorphModelVizSettingsDetails.h"

namespace UE::NearestNeighborModel
{
	class FNearestNeighborEditorModel;

	class NEARESTNEIGHBORMODELEDITOR_API FNearestNeighborModelVizSettingsDetails
		: public ::UE::MLDeformer::FMLDeformerMorphModelVizSettingsDetails
	{
	public:
		/** Makes a new instance of this detail layout class for a specific detail view requesting it. */
		static TSharedRef<IDetailCustomization> MakeInstance()
		{
			return MakeShareable(new FNearestNeighborModelVizSettingsDetails());
		}
		
		virtual void AddAdditionalSettings() override;
		FNearestNeighborEditorModel* GetCastEditorModel();
	};
}	// namespace UE::NearestNeighborModel
