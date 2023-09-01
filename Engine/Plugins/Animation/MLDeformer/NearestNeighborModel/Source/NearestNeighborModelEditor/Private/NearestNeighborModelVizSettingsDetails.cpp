// Copyright Epic Games, Inc. All Rights Reserved.

#include "NearestNeighborModelVizSettingsDetails.h"

#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailsView.h"
#include "IDetailGroup.h"
#include "NearestNeighborEditorModel.h"
#include "NearestNeighborModelVizSettings.h"

#define LOCTEXT_NAMESPACE "MLDeformerMorphModelVizSettingsDetails"

namespace UE::NearestNeighborModel
{
	void FNearestNeighborModelVizSettingsDetails::AddAdditionalSettings()
	{
		FMLDeformerMorphModelVizSettingsDetails::AddAdditionalSettings();
		IDetailGroup& NNGroup = LiveSettingsCategory->AddGroup("Nearest Neighbor", LOCTEXT("NearestNeighborLabel", "Nearest Neighbor"), false, true);
		NNGroup.AddPropertyRow(DetailLayoutBuilder->GetProperty(UNearestNeighborModelVizSettings::GetNearestNeighborActorsOffsetPropertyName(), UNearestNeighborModelVizSettings::StaticClass()));
		NNGroup.AddPropertyRow(DetailLayoutBuilder->GetProperty(UNearestNeighborModelVizSettings::GetNearestNeighborIdsPropertyName(), UNearestNeighborModelVizSettings::StaticClass()));

		IDetailGroup& ToolsGroup = LiveSettingsCategory->AddGroup("Tools", LOCTEXT("ToolsLabel", "Tools"), false, true);
		ToolsGroup.AddWidgetRow()
		.WholeRowContent()
		[
			SNew(SButton)
			.Text(FText::FromString("Get Neighbor Stats"))
			.HAlign(HAlign_Center)
			.OnClicked_Lambda([this]
			{
				if (GetCastEditorModel())
				{
					GetCastEditorModel()->GetNeighborStats();
				}
				return FReply::Handled();
			})
		];
	}

	FNearestNeighborEditorModel* FNearestNeighborModelVizSettingsDetails::GetCastEditorModel()
	{
		return static_cast<FNearestNeighborEditorModel*>(EditorModel);
	}

};
#undef LOCTEXT_NAMESPACE
