// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMEDefs.h"

#define LOCTEXT_NAMESPACE "MaterialDesignerEditorDefs"

bool UE::DynamicMaterialEditor::IsCustomMaterialProperty(EDMMaterialPropertyType InMaterialProperty)
{
	return (InMaterialProperty >= EDMMaterialPropertyType::Custom1 && InMaterialProperty <= EDMMaterialPropertyType::Custom4);
}

int32 UE::DynamicMaterialEditor::ChannelIndexToChannelBit(int32 InChannelIndex)
{
	switch (InChannelIndex)
	{
		case 0:
			return FDMMaterialStageConnectorChannel::WHOLE_CHANNEL;

		case 1:
			return FDMMaterialStageConnectorChannel::FIRST_CHANNEL;

		case 2:
			return FDMMaterialStageConnectorChannel::SECOND_CHANNEL;

		case 3:
			return FDMMaterialStageConnectorChannel::THIRD_CHANNEL;

		case 4:
			return FDMMaterialStageConnectorChannel::FOURTH_CHANNEL;

		default:
			checkNoEntry();
			return 0;
	}
}

int32 UE::DynamicMaterialEditor::ChannelBitToChannelIndex(int32 InChannelBit)
{
	switch (InChannelBit)
	{
		case FDMMaterialStageConnectorChannel::WHOLE_CHANNEL:
			return 0;

		case FDMMaterialStageConnectorChannel::FIRST_CHANNEL:
			return 1;

		case FDMMaterialStageConnectorChannel::SECOND_CHANNEL:
			return 2;

		case FDMMaterialStageConnectorChannel::THIRD_CHANNEL:
			return 3;

		case FDMMaterialStageConnectorChannel::FOURTH_CHANNEL:
			return 4;

		default:
			return 0;
	}
}

#undef LOCTEXT_NAMESPACE
