// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"

#include "CustomizableObjectFactory.generated.h"

enum class ECheckBoxState : uint8;

namespace ESelectInfo { enum Type : int; }
namespace ETextCommit { enum Type : int; }

class FFeedbackContext;
class FReply;
class STextComboBox;
class SWindow;
class UClass;
class UCustomizableObject;
class UObject;
class USkeletalMesh;

struct EVisibility;


struct FCustomizableObjectOptions
{
	// Determines if the CO asset to create has not been configurated
	bool bEmptyObject = true;

	// Child Object Settings
	// Determines if the CO asset to create is child of another CO
	bool bIsChildObject = false;

	// Parent of the CO
	TWeakObjectPtr<UCustomizableObject> ParentObject = nullptr;

	// Name of the group node of the parent where this CO asset will be linked
	FString GroupNodeName;

	// Non-Child Object Settings
	// Number of components
	int32 NumMeshComponents = 1;

	struct FComponentInfo 
	{
		TSoftObjectPtr<USkeletalMesh> ReferenceSkeletalMesh;
		FName ComponentName;
	};

	TArray<FComponentInfo> ComponentsInfo;
};


UCLASS(MinimalAPI)
class UCustomizableObjectFactory : public UFactory
{
	GENERATED_BODY()

	UCustomizableObjectFactory();

public:
	// Begin UFactory Interface
	virtual bool ConfigureProperties() override;
	virtual bool DoesSupportClass(UClass * Class) override;
	virtual UClass* ResolveSupportedClass() override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn) override;
	
	FCustomizableObjectOptions CreationSettings;
};


class FCustomizableObjectFactoryUI : public TSharedFromThis<FCustomizableObjectFactoryUI>
{
public:

	// Generates the Window widget and its content
	const FCustomizableObjectOptions ConstructFactoryUI();

	// Callback that generates a CO asset using the options set
	FReply OnCreate(bool bIsEmpty = true);

	// Creates a the tooltip of the OK button
	FText GetOKButtonTooltip() const;

	// Callback that cancels the generation of the CO asset
	FReply OnCancel();

	// Enables or disables the OK button in function of the options set.
	bool IsConfigurationValid() const;

	// Returns true if the CO asset can be created when clossing the window
	bool CanCreateObject() const;

	// Callback to set if the CO is a child object
	void OnCheckBoxChanged(ECheckBoxState State);

	// Parent Selecto Widget methods
	EVisibility GetParentWidgetsVisibility() const;
	FString GetSelectedCustomizableObjectPath() const;
	void OnPickedCustomizableObjectParent(const FAssetData& SelectedAsset);
	void OnSelectGroupComboBox(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo);
	void GenerateGroupOptions();
	bool IsNodeGroupSelectorEnabled() const;

	// Component Selectior Widget methods
	EVisibility GetComponentWidgetsVisibility() const;
	FText GetSelectorWidgetText(bool bIsName) const;
	FString GetSelectedComponentSkeletalMeshPath() const;
	int32 GetNumComponents() const;
	void OnNumComponentsChanged(int32 Value, ETextCommit::Type CommitInfo);
	void OnSelectComponentComboBox(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo);
	void OnPickedComponentSkeletalMesh(const FAssetData& SelectedAsset);
	void GenerateComponentOptions();
	bool IsComponentSelectorEnabled() const;

	FText GetComponentName() const;
	void OnTextCommited(const FText& NewName, ETextCommit::Type CommitInfo);

	int32 GetSelectedComponentIndex() const;

private:

	// Pointer to the created widgets
	TSharedPtr<SWindow> COSettingsWindow;
	TSharedPtr<STextComboBox> GroupSelector;
	TSharedPtr<STextComboBox> ComponentSelector;

	// bool that determines if a CO asset can be created
	bool bCreateObject = false;

	// Setting options to generate the CO asset
	FCustomizableObjectOptions Options;

	// Combobox options
	TArray<TSharedPtr<FString>> GroupOptions;
	TArray<TSharedPtr<FString>> ComponentsOptions;

};
