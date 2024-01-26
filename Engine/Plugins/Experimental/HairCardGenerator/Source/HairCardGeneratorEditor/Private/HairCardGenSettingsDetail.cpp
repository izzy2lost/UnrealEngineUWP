// Copyright Epic Games, Inc. All Rights Reserved.

#include "HairCardGenSettingsDetail.h"
#include "HairCardGeneratorPluginSettings.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/STextBlock.h"

#include "HairCardGeneratorLog.h"

#define LOCTEXT_NAMESPACE "HairCardSettingsDetails"

void FHairCardSettingsDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
    TArray<TWeakObjectPtr<UHairCardGeneratorPluginSettings>> DetailObjects = DetailBuilder.GetObjectsOfTypeBeingCustomized<UHairCardGeneratorPluginSettings>();
    if ( DetailObjects.Num() != 1 || !DetailObjects[0].IsValid() )
    {
        SettingsPtr = nullptr;    
        return;
    }

    // Only support single object customized view (this is fine as HCS is a modal dialog anyway)
    SettingsPtr = DetailObjects[0];

    DetailBuilder.EditCategory(TEXT("Asset"));
    DetailBuilder.EditCategory(TEXT("Import"));

    IDetailCategoryBuilder& LodCategory = DetailBuilder.EditCategory(TEXT("Level Of Detail"));
    const TSharedPtr<IPropertyHandle> ReduceFromLODHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UHairCardGeneratorPluginSettings, bReduceCardsFromPreviousLOD));
    const TSharedPtr<IPropertyHandle> UseReservedTxHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UHairCardGeneratorPluginSettings, bUseReservedSpaceFromPreviousLOD));

    DetailBuilder.EditCategory(TEXT("Randomness"));
    IDetailCategoryBuilder& TxRenderCategory = DetailBuilder.EditCategory(TEXT("Texture Rendering"));

    // Specialized widget with dynamic tooltip for enabled/disabled derive from previous LOD checkbox
    DetailBuilder.HideProperty(ReduceFromLODHandle);
    LodCategory.AddCustomRow(LOCTEXT("ReduceFromLOD.Filter", "Reduce Cards from Previous LOD"))
    .NameContent()
    [
            SNew(STextBlock)
            .Text(LOCTEXT("ReduceFromLOD.Name", "Reduce Cards from Previous LOD"))
            .ToolTipText(ReduceFromLODHandle->GetToolTipText())
            .Font(IDetailLayoutBuilder::GetDetailFont())
    ]
    .ValueContent()
    [
        SNew(SCheckBox)
        .IsChecked(this, &FHairCardSettingsDetailCustomization::GetCheckValue, ReduceFromLODHandle)
        .OnCheckStateChanged(this, &FHairCardSettingsDetailCustomization::SetCheckValue, ReduceFromLODHandle)
        .IsEnabled(this, &FHairCardSettingsDetailCustomization::CheckReduceFromLOD)
        .ToolTipText(this, &FHairCardSettingsDetailCustomization::ToolTipReduceFromLOD, ReduceFromLODHandle)
    ];

    // Specialized widget with dynamic tooltip for enabled/disabled use previous reserved texture space
    DetailBuilder.HideProperty(UseReservedTxHandle);
    TxRenderCategory.AddCustomRow(LOCTEXT("UseReservedTx.Filter", "Use Reserved Space from Previous LOD"))
    .NameContent()
    [
            SNew(STextBlock)
            .Text(LOCTEXT("UseReservedTx.Name", "Use Reserved Space from Previous LOD"))
            .ToolTipText(UseReservedTxHandle->GetToolTipText())
            .Font(IDetailLayoutBuilder::GetDetailFont())
    ]
    .ValueContent()
    [
        SNew(SCheckBox)
        .IsChecked(this, &FHairCardSettingsDetailCustomization::GetCheckValue, UseReservedTxHandle)
        .OnCheckStateChanged(this, &FHairCardSettingsDetailCustomization::SetCheckValue, UseReservedTxHandle)
        .IsEnabled(this, &FHairCardSettingsDetailCustomization::CheckUseReservedTx)
        .ToolTipText(this, &FHairCardSettingsDetailCustomization::ToolUseReservedTx, UseReservedTxHandle)
    ];
}


TSharedRef<IDetailCustomization> FHairCardSettingsDetailCustomization::MakeInstance()
{
    return MakeShareable(new FHairCardSettingsDetailCustomization());
}



ECheckBoxState FHairCardSettingsDetailCustomization::GetCheckValue(const TSharedPtr<IPropertyHandle> Property) const
{
    bool CheckVal;
    if ( Property.IsValid() && Property->GetValue(CheckVal) )
    {
        return (CheckVal) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
    }

    return ECheckBoxState::Undetermined;
}

void FHairCardSettingsDetailCustomization::SetCheckValue(ECheckBoxState NewState, TSharedPtr<IPropertyHandle> Property) const
{
    if ( Property.IsValid() )
    {
        Property->SetValue(NewState == ECheckBoxState::Checked);
    }
}

bool FHairCardSettingsDetailCustomization::CheckReduceFromLOD() const
{
    if ( !SettingsPtr.IsValid() )
    {
        return false;
    }

    if ( SettingsPtr.Get()->GetLODIndex() < 1 )
    {
        return false;
    }
    
    if ( !SettingsPtr.Get()->HasValidFullParent() )
    {
        return false;;
    }

    return true;
}

FText FHairCardSettingsDetailCustomization::ToolTipReduceFromLOD(const TSharedPtr<IPropertyHandle> Property) const
{
    if ( !SettingsPtr.IsValid() )
    {
        return Property->GetToolTipText();
    }

    if ( SettingsPtr.Get()->GetLODIndex() < 1 )
    {
        return LOCTEXT("ReduceFromLOD.LOD0.ToolTip", "Cannot reduce LOD 0 (must run full generation)");
    }

    if ( !SettingsPtr.Get()->HasValidFullParent() )
    {
        return LOCTEXT("ReduceFromLOD.InvalidParent.ToolTip", "All lower LODs must be generated using the hair card generator tool");
    }

    return Property->GetToolTipText();
}


bool FHairCardSettingsDetailCustomization::CheckUseReservedTx() const
{
    if ( !SettingsPtr.IsValid() )
    {
        return false;
    }

    if ( SettingsPtr->bReduceCardsFromPreviousLOD )
    {
        return false;
    }

    if ( !SettingsPtr->HasDerivedTextureSettings() )
    {
        return false;
    }

    // TODO: Handle limiting/reserving texture by using texture resolution to compute reserved space in pixels
    return (SettingsPtr->GetDerivedReservedTextureSize() >= 5);
}

FText FHairCardSettingsDetailCustomization::ToolUseReservedTx(const TSharedPtr<IPropertyHandle> Property) const
{
    if ( !SettingsPtr.IsValid() )
    {
        return Property->GetToolTipText();
    }

    if ( SettingsPtr->bReduceCardsFromPreviousLOD )
    {
        return LOCTEXT("UseReservedTx.Reducing.ToolTip", "Reduced card geometry will use previous LOD texture UVs");
    }

    if ( !SettingsPtr->HasDerivedTextureSettings() )
    {
        return LOCTEXT("UseReservedTx.InvalidParent.ToolTip", "Lower LODs must be generated using hair card generator tool with reserved space");
    }

    if ( SettingsPtr->GetDerivedReservedTextureSize() < 5 )
    {
        return LOCTEXT("UseReservedTx.NoReservedSpace.ToolTip", "No space reserved in parent LOD chain");
    }

    return Property->GetToolTipText();
}


#undef LOCTEXT_NAMESPACE
