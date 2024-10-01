// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/SceneComponent.h"

#include "MuCO/CustomizableObjectInstance.h"

#include "CustomizableSkeletalComponent.generated.h"

class UCustomizableSkeletalComponentPrivate;

DECLARE_DELEGATE(FCustomizableSkeletalComponentUpdatedDelegate);


UCLASS(Blueprintable, BlueprintType, ClassGroup = (CustomizableObject), meta = (BlueprintSpawnableComponent))
class CUSTOMIZABLEOBJECT_API UCustomizableSkeletalComponent : public USceneComponent
{
public:
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = CustomizableSkeletalComponent)
	TObjectPtr<UCustomizableObjectInstance> CustomizableObjectInstance;

	/** This component index refers to the object list of components */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = CustomizableSkeletalComponent)
	int32 ComponentIndex;

private:
	/** Only used if the ComponentIndex is INDEX_NONE.
	 *  Editing this property will set ComponentIndex to INDEX_NONE. */
	UPROPERTY(EditAnywhere, Category = CustomizableSkeletalComponent)
	FName ComponentName;

public: // TODO GMT Private
	UPROPERTY(EditAnywhere, Category = CustomizableSkeletalComponent)
	bool bSkipSetReferenceSkeletalMesh = false;
	
public:
	FCustomizableSkeletalComponentUpdatedDelegate UpdatedDelegate;

private:
	UPROPERTY()
	TObjectPtr<UCustomizableSkeletalComponentPrivate> Private;

public:
	UCustomizableSkeletalComponent();
	
	// UObject interface
protected:
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	// UActor interface
	virtual void PostInitProperties() override;
	virtual void PostReinitProperties() override;

	// USceneComponent interface
	virtual void OnAttachmentChanged() override;

public:
	// Own interface
	
	UFUNCTION(BlueprintCallable, Category = CustomizableSkeletalComponent)
	void SetComponentName(const FName& Name);

	UFUNCTION(BlueprintCallable, Category = CustomizableSkeletalComponent)
	FName GetComponentName() const;
	
	UFUNCTION(BlueprintCallable, Category = CustomizableSkeletalComponent)
	UCustomizableObjectInstance* GetCustomizableObjectInstance() const;
	
	UFUNCTION(BlueprintCallable, Category = CustomizableSkeletalComponent)
	void SetCustomizableObjectInstance(UCustomizableObjectInstance* Instance);
	
	/** Set to true to avoid automatically replacing the Skeletal Mesh of the parent Skeletal Mesh Component by the Reference Skeletal Mesh.
	 * If SkipSetSkeletalMeshOnAttach is true, it will not replace it. */
	UFUNCTION(BlueprintCallable, Category = CustomizableSkeletalComponent)
	void SetSkipSetReferenceSkeletalMesh(bool bSkip);

	UFUNCTION(BlueprintCallable, Category = CustomizableSkeletalComponent)
	bool GetSkipSetReferenceSkeletalMesh() const;

	/** Set to true to avoid automatically replacing the Skeletal Mesh of the parent Skeletal Mesh Component with any mesh. */
	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstanceUsage)
	void SetSkipSetSkeletalMeshOnAttach(bool bSkip);

	UFUNCTION(BlueprintCallable, Category = CustomizableObjectInstanceUsage)
	bool GetSkipSetSkeletalMeshOnAttach() const;
	
	/** Update Skeletal Mesh asynchronously. */
	UFUNCTION(BlueprintCallable, Category = CustomizableSkeletalComponent)
	void UpdateSkeletalMeshAsync(bool bNeverSkipUpdate = false);

	/** Update Skeletal Mesh asynchronously. Callback will be called once the update finishes, even if it fails. */
	UFUNCTION(BlueprintCallable, Category = CustomizableSkeletalComponent)
	void UpdateSkeletalMeshAsyncResult(FInstanceUpdateDelegate Callback, bool bIgnoreCloseDist = false, bool bForceHighPriority = false);

	UCustomizableSkeletalComponentPrivate* GetPrivate();

	const UCustomizableSkeletalComponentPrivate* GetPrivate() const;
	
private:
	UPROPERTY(Transient)
	TObjectPtr<UCustomizableObjectInstanceUsage> CustomizableObjectInstanceUsage;
};

