// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/GenerateMutableSource/GenerateMutableSourceTable.h"

#include "Animation/AnimInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2DArray.h"
#include "GameplayTagContainer.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "MuCO/CustomizableObjectSystem.h"
#include "MuCO/UnrealPortabilityHelpers.h"
#include "MuCOE/CustomizableObjectCompiler.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTable.h"
#include "MuR/Mesh.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


bool FillTableColumn(const UCustomizableObjectNodeTable* TableNode,	mu::TablePtr MutableTable,	const FString& ColumnName,	const FString& RowName,	const int32 RowIdx,	uint8* CellData, const FProperty* ColumnProperty,
	const int LODIndexConnected, const int32 SectionIndexConnected, int32 LODIndex, int32 SectionIndex, const bool bOnlyConnectedLOD, FMutableGraphGenerationContext& GenerationContext)
{
	int32 CurrentColumn;

	// Getting property type
	if (const FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(ColumnProperty))
	{
		UObject* Object = SoftObjectProperty->GetPropertyValue(CellData).LoadSynchronous();

		if (SoftObjectProperty->PropertyClass->IsChildOf(USkeletalMesh::StaticClass()))
		{
			USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Object);

			if(!SkeletalMesh)
			{
				// Generating an Empty cell
				FString MutableColumnName = TableNode->GenerateSkeletalMeshMutableColumName(ColumnName, LODIndex, SectionIndex);

				CurrentColumn = MutableTable.get()->FindColumn(MutableColumnName);

				if (CurrentColumn == -1)
				{
					CurrentColumn = MutableTable->AddColumn(MutableColumnName, mu::ETableColumnType::Mesh);
				}

				mu::MeshPtr EmptySkeletalMesh = nullptr;
				MutableTable->SetCell(CurrentColumn, RowIdx, EmptySkeletalMesh.get());

				return true;
			}

			// Getting Animation Blueprint and Animation Slot
			FString AnimBP, AnimSlot, GameplayTag, AnimBPAssetTag;
			TArray<FGameplayTag> GameplayTags;

			TableNode->GetAnimationColumns(ColumnName, AnimBP, AnimSlot, GameplayTag);

			if (!AnimBP.IsEmpty())
			{
				if (!AnimSlot.IsEmpty())
				{
					if (TableNode->Table)
					{
						uint8* AnimRowData = TableNode->Table->FindRowUnchecked(FName(*RowName));

						if (AnimRowData)
						{
							FName SlotIndex;

							// Getting animation slot row value from data table
							if (FProperty* AnimSlotProperty = TableNode->Table->FindTableProperty(FName(*AnimSlot)))
							{
								uint8* AnimSlotData = AnimSlotProperty->ContainerPtrToValuePtr<uint8>(AnimRowData, 0);

								if (AnimSlotData)
								{
									if (const FIntProperty* IntProperty = CastField<FIntProperty>(AnimSlotProperty))
									{
										FString Message = FString::Printf(
											TEXT("The column with name [%s] for the Anim Slot property should be an FName instead of an Integer, it will be internally converted to FName but should probaly be converted in the table itself."), 
											*AnimBP);
										GenerationContext.Compiler->CompilerLog(FText::FromString(Message), TableNode, EMessageSeverity::Info);

										SlotIndex = FName(FString::FromInt(IntProperty->GetPropertyValue(AnimSlotData)));
									}
									else if (const FNameProperty* NameProperty = CastField<FNameProperty>(AnimSlotProperty))
									{
										SlotIndex = NameProperty->GetPropertyValue(AnimSlotData);
									}
								}
							}

							if (SlotIndex.GetStringLength() != 0)
							{
								// Getting animation instance soft class from data table
								if (FProperty* AnimBPProperty = TableNode->Table->FindTableProperty(FName(*AnimBP)))
								{
									uint8* AnimBPData = AnimBPProperty->ContainerPtrToValuePtr<uint8>(AnimRowData, 0);

									if (AnimBPData)
									{
										if (const FSoftClassProperty* SoftClassProperty = CastField<FSoftClassProperty>(AnimBPProperty))
										{
											TSoftClassPtr<UAnimInstance> AnimInstance(SoftClassProperty->GetPropertyValue(AnimBPData).ToSoftObjectPath());

											if (!AnimInstance.IsNull())
											{
												if (UClass* Anim = AnimInstance.LoadSynchronous())
												{
													GenerationContext.AddParticipatingObject(*Anim);													
												}

												GenerationContext.AnimBPAssetsMap.Add(AnimInstance.ToString(), AnimInstance);

												AnimBPAssetTag = GenerateAnimationInstanceTag(AnimInstance.ToString(), SlotIndex);
											}
										}
									}
								}
							}
							else
							{
								FString msg = FString::Printf(TEXT("Could not find the Slot column of the animation blueprint column [%s] for the mesh column [%s] row [%s]."), *AnimBP, *ColumnName, *RowName);
								GenerationContext.Compiler->CompilerLog(FText::FromString(msg), TableNode);
							}
						}
					}
				}
				else
				{
					FString msg = FString::Printf(TEXT("Could not found the Slot column of the animation blueprint column [%s] for the mesh column [%s]."), *AnimBP, *ColumnName);
					GenerationContext.Compiler->CompilerLog(FText::FromString(msg), TableNode);
				}
			}

			// Getting Gameplay tags
			if (!GameplayTag.IsEmpty())
			{
				if (TableNode->Table)
				{
					uint8* GameplayRowData = TableNode->Table->FindRowUnchecked(FName(*RowName));

					if (GameplayRowData)
					{
						// Getting animation tag row value from data table
						if (FProperty* GameplayTagProperty = TableNode->Table->FindTableProperty(FName(*GameplayTag)))
						{
							uint8* GameplayTagData = GameplayTagProperty->ContainerPtrToValuePtr<uint8>(GameplayRowData, 0);

							if (const FStructProperty* StructProperty = CastField<FStructProperty>(GameplayTagProperty))
							{
								if (StructProperty->Struct == TBaseStructure<FGameplayTagContainer>::Get())
								{
									if (GameplayTagData)
									{
										const FGameplayTagContainer TagContainer = *(FGameplayTagContainer*)GameplayTagData;
										TagContainer.GetGameplayTagArray(GameplayTags);
									}
								}
							}
						}
					}
				}
			}

			// Getting reference Mesh
			USkeletalMesh* ReferenceSkeletalMesh = TableNode->GetColumnDefaultAssetByType<USkeletalMesh>(ColumnName);

			if (!ReferenceSkeletalMesh)
			{
				FString msg = FString::Printf(TEXT("Reference Skeletal Mesh not found for column [%s]."), *ColumnName);
				GenerationContext.Compiler->CompilerLog(FText::FromString(msg), TableNode);

				return false;
			}

			GetLODAndSectionForAutomaticLODs(GenerationContext, *TableNode, *SkeletalMesh, LODIndexConnected, SectionIndexConnected, LODIndex, SectionIndex, bOnlyConnectedLOD);
			
			// Parameter used for LOD differences
	
			if (GenerationContext.CurrentAutoLODStrategy != ECustomizableObjectAutomaticLODStrategy::AutomaticFromMesh || 
				SectionIndex == SectionIndexConnected)
			{
				const int32 NumLODs = SkeletalMesh->GetImportedModel()->LODModels.Num();

				if (NumLODs <= LODIndex)
				{
					LODIndex = NumLODs - 1;

					FString msg = FString::Printf(TEXT("Mesh from column [%s] row [%s] needs LOD %d but has less LODs than the reference mesh. LOD %d will be used instead. This can cause some performance penalties."),
						*ColumnName, *RowName, LODIndex, LODIndex);
					GenerationContext.Compiler->CompilerLog(FText::FromString(msg), TableNode);
				}
			}

			FSkeletalMeshModel* ImportedModel = SkeletalMesh->GetImportedModel();
			
			if (ImportedModel->LODModels.IsValidIndex(LODIndex)) // Ignore error since this Section is empty due to Automatic LODs From Mesh
			{
				int32 NumSections = ImportedModel->LODModels[LODIndex].Sections.Num();
				int32 ReferenceNumMaterials = ImportedModel->LODModels[LODIndex].Sections.Num();

				if (NumSections != ReferenceNumMaterials)
				{
					FString Dif_1 = NumSections > ReferenceNumMaterials ? "more" : "less";
					FString Dif_2 = NumSections > ReferenceNumMaterials ? "Some will be ignored" : "This can cause some compilation errors.";

					FString msg = FString::Printf(TEXT("Mesh from column [%s] row [%s] has %s Sections than the reference mesh. %s"), *ColumnName, *RowName, *Dif_1, *Dif_2);
					GenerationContext.Compiler->CompilerLog(FText::FromString(msg), TableNode);
				}
			}

			FString MutableColumnName = TableNode->GenerateSkeletalMeshMutableColumName(ColumnName, LODIndex, SectionIndex);

			CurrentColumn = MutableTable.get()->FindColumn(MutableColumnName);

			if (CurrentColumn == -1)
			{
				CurrentColumn = MutableTable->AddColumn(MutableColumnName, mu::ETableColumnType::Mesh);
			}

			// First process the mesh tags that are going to make the mesh unique and affect whether it's repeated in 
			// the mesh cache or not
			FString MeshUniqueTags;

			if (!AnimBPAssetTag.IsEmpty())
			{
				MeshUniqueTags += AnimBPAssetTag;
			}

			TArray<FString> ArrayAnimBPTags;

			for (const FGameplayTag& Tag : GameplayTags)
			{
				MeshUniqueTags += GenerateGameplayTag(Tag.ToString());
			}

			//TODO: Add AnimBp physics to Tables.
			mu::Ptr<mu::Mesh> MutableMesh = GenerateMutableMesh(SkeletalMesh, TSoftClassPtr<UAnimInstance>(), LODIndexConnected, SectionIndexConnected, LODIndex, SectionIndex, MeshUniqueTags, GenerationContext, TableNode);

			if (MutableMesh)
			{
 				if (SkeletalMesh->GetPhysicsAsset() && MutableMesh->GetPhysicsBody() && MutableMesh->GetPhysicsBody()->GetBodyCount())
				{
 					TSoftObjectPtr<UPhysicsAsset> PhysicsAsset = SkeletalMesh->GetPhysicsAsset();

					GenerationContext.AddParticipatingObject(*PhysicsAsset); 						

					GenerationContext.PhysicsAssetMap.Add(PhysicsAsset.ToString(), PhysicsAsset);
					FString PhysicsAssetTag = FString("__PhysicsAsset:") + PhysicsAsset.ToString();

					AddTagToMutableMeshUnique(*MutableMesh, PhysicsAssetTag);
				}

				if (!AnimBPAssetTag.IsEmpty())
				{
					AddTagToMutableMeshUnique(*MutableMesh, AnimBPAssetTag);
				}

				for (const FGameplayTag& Tag : GameplayTags)
				{
					AddTagToMutableMeshUnique(*MutableMesh, GenerateGameplayTag(Tag.ToString()));
				}

				if (GenerationContext.Object->bEnableAssetUserDataMerge)
				{
					const TArray<UAssetUserData*>* AssetUserDataArray = SkeletalMesh->GetAssetUserDataArray();

					if (AssetUserDataArray && !AssetUserDataArray->IsEmpty())
					{
						for (const UAssetUserData* AssetUserData : *AssetUserDataArray)
						{
							FString AuxString = AssetUserData->GetPathName();
							GenerationContext.AssetUserDataAssetsMap.Add(AuxString, TSoftObjectPtr<UAssetUserData>(AssetUserData));

							FString AssetUserDataTag = GenerateAssetUserDataTag(AuxString);
							AddTagToMutableMeshUnique(*MutableMesh, AssetUserDataTag);
						}
					}
				}

				AddSocketTagsToMesh(SkeletalMesh, MutableMesh, GenerationContext);

				if (UCustomizableObjectSystem::GetInstance()->IsMutableAnimInfoDebuggingEnabled())
				{
					FString MeshPath;
					SkeletalMesh->GetOuter()->GetPathName(nullptr, MeshPath);
					FString MeshTag = FString("__MeshPath:") + MeshPath;
					AddTagToMutableMeshUnique(*MutableMesh, MeshTag);
				}

				MutableTable->SetCell(CurrentColumn, RowIdx, MutableMesh.get(), SkeletalMesh);
			}
			else
			{
				FString msg = FString::Printf(TEXT("Error converting skeletal mesh LOD %d, Section %d from column [%s] row [%s] to mutable."),
					LODIndex, SectionIndex, *ColumnName, *RowName);
				GenerationContext.Compiler->CompilerLog(FText::FromString(msg), TableNode);
			}
		}

		else if (SoftObjectProperty->PropertyClass->IsChildOf(UStaticMesh::StaticClass()))
		{
			UStaticMesh* StaticMesh = Cast<UStaticMesh>(Object);

			if (!StaticMesh)
			{
				return false;
			}

			// Getting reference Mesh
			UStaticMesh* ReferenceStaticMesh = TableNode->GetColumnDefaultAssetByType<UStaticMesh>(ColumnName);

			if (!ReferenceStaticMesh)
			{
				FString msg = FString::Printf(TEXT("Reference Static Mesh not found for column [%s]."), *ColumnName);
				GenerationContext.Compiler->CompilerLog(FText::FromString(msg), TableNode);

				return false;
			}

			// Parameter used for LOD differences
			int32 CurrentLOD = LODIndex;

			int NumLODs = StaticMesh->GetRenderData()->LODResources.Num();

			if (NumLODs <= CurrentLOD)
			{
				CurrentLOD = NumLODs - 1;

				FString msg = FString::Printf(TEXT("Mesh from column [%s] row [%s] needs LOD %d but has less LODs than the reference mesh. LOD %d will be used instead. This can cause some performance penalties."),
					*ColumnName, *RowName, LODIndex, CurrentLOD);
				GenerationContext.Compiler->CompilerLog(FText::FromString(msg), TableNode);
			}

			int32 NumMaterials = StaticMesh->GetRenderData()->LODResources[CurrentLOD].Sections.Num();
			int32 ReferenceNumMaterials = ReferenceStaticMesh->GetRenderData()->LODResources[CurrentLOD].Sections.Num();

			if (NumMaterials != ReferenceNumMaterials)
			{
				FString FirstTextOption = NumMaterials > ReferenceNumMaterials ? "more" : "less";
				FString SecondTextOption = NumMaterials > ReferenceNumMaterials ? "Some will be ignored" : "This can cause some compilation errors.";

				FString msg = FString::Printf(TEXT("Mesh from column [%s] row [%s] has %s Sections than the reference mesh. %s"), *ColumnName, *RowName, *FirstTextOption, *SecondTextOption);
				GenerationContext.Compiler->CompilerLog(FText::FromString(msg), TableNode);
			}

			FString MutableColumnName = TableNode->GenerateStaticMeshMutableColumName(ColumnName, SectionIndex);

			CurrentColumn = MutableTable.get()->FindColumn(MutableColumnName);

			if (CurrentColumn == -1)
			{
				CurrentColumn = MutableTable->AddColumn(MutableColumnName, mu::ETableColumnType::Mesh);
			}

			mu::MeshPtr MutableMesh = GenerateMutableMesh(StaticMesh, TSoftClassPtr<UAnimInstance>(), CurrentLOD, SectionIndex, CurrentLOD, SectionIndex, FString(), GenerationContext, TableNode);

			if (MutableMesh)
			{
				MutableTable->SetCell(CurrentColumn, RowIdx, MutableMesh.get(), StaticMesh);
			}
			else
			{
				FString msg = FString::Printf(TEXT("Error converting skeletal mesh LOD %d, Section %d from column [%s] row [%s] to mutable."),
					LODIndex, SectionIndex, *ColumnName, *RowName);

				GenerationContext.Compiler->CompilerLog(FText::FromString(msg), TableNode);
			}
		}

		else if (SoftObjectProperty->PropertyClass->IsChildOf(UTexture::StaticClass()))
		{
			// Two supported texture types
			UTexture2D* Texture2D = Cast<UTexture2D>(Object);
			UTexture2DArray* TextureArray = Cast<UTexture2DArray>(Object);

			if (!Texture2D && !TextureArray)
			{
				Texture2D = TableNode->GetColumnDefaultAssetByType<UTexture2D>(ColumnName);
				TextureArray = TableNode->GetColumnDefaultAssetByType<UTexture2DArray>(ColumnName);

				FString Message = Cast<UObject>(Object) ? "not a suported Texture" : "null";
				FString WarningMessage = FString::Printf(TEXT("Texture from column [%s] row [%s] is %s. The default texture will be used instead."), *ColumnName, *RowName, *Message);
				GenerationContext.Compiler->CompilerLog(FText::FromString(WarningMessage), TableNode);
			}

			// There will be always one of the two options
			check(Texture2D || TextureArray);
			UTexture* Texture = Texture2D ? Cast<UTexture>(Texture2D) : Cast<UTexture>(TextureArray);

			// Getting column index from column name
			CurrentColumn = MutableTable->FindColumn(ColumnName);

			if (CurrentColumn == INDEX_NONE)
			{
				CurrentColumn = MutableTable->AddColumn(ColumnName, mu::ETableColumnType::Image);
			}

			// Removing encoding part
			const FString PinName = ColumnName.Replace(TEXT("--PassThrough"),TEXT(""), ESearchCase::CaseSensitive);

			if (TableNode->GetColumnImageMode(PinName) == ETableTextureType::PASSTHROUGH_TEXTURE)
			{
				FMutableGraphGenerationContext::FGeneratedPassThroughTexture* FoundIndex = GenerationContext.PassThroughTextureMap.Find(Texture);
				FMutableGraphGenerationContext::FGeneratedPassThroughTexture NewEntry;

				if (!FoundIndex)
				{
					FoundIndex = &NewEntry;
					NewEntry.ID = GenerationContext.PassThroughTextureMap.Num();
					NewEntry.ImageDesc.m_size[0] = Texture->Source.GetSizeX();
					NewEntry.ImageDesc.m_size[1] = Texture->Source.GetSizeY();
					NewEntry.ImageDesc.m_lods = Texture->Source.GetNumMips();
					NewEntry.ImageDesc.m_format = mu::EImageFormat::IF_RGBA_UBYTE; //TODO: it cannot be known without actually loading the image, which we don't want to do here.
					GenerationContext.PassThroughTextureMap.Add(Texture, NewEntry);
				}

				mu::Ptr<mu::ResourceProxyMemory<mu::Image>> Proxy = new mu::ResourceProxyMemory<mu::Image>(mu::Image::CreateAsReference(FoundIndex->ID,FoundIndex->ImageDesc, false));
				MutableTable->SetCell(CurrentColumn, RowIdx, Proxy.get());
			}
			else 
			{
				if (Texture2D)
				{
					GenerationContext.AddParticipatingObject(*Texture2D);
				}

				if (GenerationContext.Options.OptimizationLevel == 0)
				{
					// In the "None" optimization level, we don't process any image, and we set them all as image references
					// that will be loaded at instance generation time: fast compilation x slow generation.
					FMutableGraphGenerationContext::FGeneratedPassThroughTexture* FoundIndex = GenerationContext.PassThroughTextureMap.Find(Texture);
					FMutableGraphGenerationContext::FGeneratedPassThroughTexture NewEntry;

					if (!FoundIndex)
					{
						FoundIndex = &NewEntry;
						NewEntry.ID = GenerationContext.PassThroughTextureMap.Num();
						NewEntry.ImageDesc.m_size[0] = Texture->Source.GetSizeX();
						NewEntry.ImageDesc.m_size[1] = Texture->Source.GetSizeY();
						NewEntry.ImageDesc.m_lods = Texture->Source.GetNumMips();
						NewEntry.ImageDesc.m_format = mu::EImageFormat::IF_RGBA_UBYTE; //TODO: it cannot be known without actually loading the image, which we don't want to do here.
						GenerationContext.PassThroughTextureMap.Add(Texture, NewEntry);
					}

					mu::Ptr<mu::ResourceProxyMemory<mu::Image>> Proxy = new mu::ResourceProxyMemory<mu::Image>(mu::Image::CreateAsReference(FoundIndex->ID, FoundIndex->ImageDesc, true));
					MutableTable->SetCell(CurrentColumn, RowIdx, Proxy.get());
				}
				else
				{
					GenerationContext.ArrayTextureUnrealToMutableTask.Add(FTextureUnrealToMutableTask(MutableTable, Texture2D, TableNode, CurrentColumn, RowIdx));
				}

			}
		}

		else if (SoftObjectProperty->PropertyClass->IsChildOf(UMaterialInstance::StaticClass()))
		{
			// Getting the name of material column of the data table
			FString MaterialColumnName = ColumnProperty->GetDisplayNameText().ToString();

			UMaterialInstance* Material = Cast<UMaterialInstance>(Object);
			UMaterialInstance* ReferenceMaterial = TableNode->GetColumnDefaultAssetByType<UMaterialInstance>(MaterialColumnName);

			if (!ReferenceMaterial)
			{
				FString msg = FString::Printf(TEXT("Reference Material not found for column [%s]."), *MaterialColumnName);
				GenerationContext.Compiler->CompilerLog(FText::FromString(msg), TableNode);

				return false;
			}

			GenerationContext.AddParticipatingObject(*ReferenceMaterial);

			if (!Material || ReferenceMaterial->GetMaterial() != Material->GetMaterial())
			{
				FText Warning;

				if (!Material)
				{
					Warning = FText::Format(LOCTEXT("NullMaterialInstance", "Material Instance from column [{0}] row [{1}] is null. The default Material Instance will be used instead."),
						FText::FromString(MaterialColumnName), FText::FromString(RowName));
				}
				else
				{
					Warning = FText::Format(LOCTEXT("MatInstanceFromDifferentParent","Material Instance from column [{0}] row [{1}] has a different Material Parent than the Default Material Instance. The Default Material Instance will be used instead."),
						FText::FromString(MaterialColumnName), FText::FromString(RowName));
				}

				Material = ReferenceMaterial;

				GenerationContext.Compiler->CompilerLog(Warning, TableNode);
			}

			GenerationContext.AddParticipatingObject(*Material);
			
			FString EncodedSwitchParameterName = "__MutableMaterialId";
			if (ColumnName.Contains(EncodedSwitchParameterName))
			{
				CurrentColumn = MutableTable.get()->FindColumn(ColumnName);

				if (CurrentColumn == -1)
				{
					CurrentColumn = MutableTable->AddColumn(ColumnName, mu::ETableColumnType::Scalar);
				}

				const int32 lastMaterialAmount = GenerationContext.ReferencedMaterials.Num();
				int32 ReferenceMaterialId = GenerationContext.ReferencedMaterials.AddUnique(Material);

				// Take slot name from skeletal mesh if one can be found, else leave empty.
				// Keep Referenced Materials and Materail Slot Names synchronized even if no material name can be found.
				const bool IsNewSlotName = GenerationContext.ReferencedMaterialSlotNames.Num() == ReferenceMaterialId;

				if (IsNewSlotName)
				{
					GenerationContext.ReferencedMaterialSlotNames.Add(FName(NAME_None));
				}

				MutableTable->SetCell(CurrentColumn, RowIdx, (float)ReferenceMaterialId);

				return true;
			}

			int32 ColumnIndex;

			// Getting parameter value
			TArray<FMaterialParameterInfo> ParameterInfos;
			TArray<FGuid> ParameterGuids;

			Material->GetMaterial()->GetAllParameterInfoOfType(EMaterialParameterType::Texture, ParameterInfos, ParameterGuids);
			
			FGuid ParameterId(GenerationContext.CurrentMaterialTableParameterId);
			int32 ParameterIndex = ParameterGuids.Find(ParameterId);

			if (ParameterIndex != INDEX_NONE && ParameterInfos[ParameterIndex].Name == GenerationContext.CurrentMaterialTableParameter)
			{
				// Getting column index from parameter name
				ColumnIndex = MutableTable->FindColumn(ColumnName);

				if (ColumnIndex == INDEX_NONE)
				{
					// If there is no column with the parameters name, we generate a new one
					ColumnIndex = MutableTable->AddColumn(ColumnName, mu::ETableColumnType::Image);
				}

				UTexture* ParentTextureValue = nullptr;
				Material->GetMaterial()->GetTextureParameterValue(ParameterInfos[ParameterIndex], ParentTextureValue);
				
				UTexture2D* ParentParameterTexture = Cast<UTexture2D>(ParentTextureValue);
				if (!ParentParameterTexture)
				{
					FString ParamName = ParameterInfos[ParameterIndex].Name.ToString();
					FString Message = Cast<UObject>(ParentParameterTexture) ? "not a Texture2D" : "null";
					
					FString msg = FString::Printf(TEXT("Parameter [%s] from Default Material Instance of column [%s] is %s. This parameter will be ignored."), *ParamName, *MaterialColumnName, *Message);
					GenerationContext.Compiler->CompilerLog(FText::FromString(msg), TableNode);
					 
					 return false;
				}

				UTexture* TextureValue = nullptr;
				Material->GetTextureParameterValue(ParameterInfos[ParameterIndex], TextureValue);

				UTexture2D* ParameterTexture = Cast<UTexture2D>(TextureValue);

				if (!ParameterTexture)
				{
					ParameterTexture = ParentParameterTexture;

					FString ParamName = GenerationContext.CurrentMaterialTableParameter;
					FString Message = Cast<UObject>(TextureValue) ? "not a Texture2D" : "null";

					FString msg = FString::Printf(TEXT("Parameter [%s] from material instance of column [%s] row [%s] is %s. The parameter texture of the default material will be used instead."), *ParamName, *MaterialColumnName, *RowName, *Message);
					GenerationContext.Compiler->CompilerLog(FText::FromString(msg), TableNode);
				}

				GenerationContext.ArrayTextureUnrealToMutableTask.Add(FTextureUnrealToMutableTask(MutableTable, ParameterTexture, TableNode, ColumnIndex, RowIdx));

				return true;
			}
		}

		else
		{
			// Unsuported Variable Type
			return false;
		}
	}

	else if (const FStructProperty* StructProperty = CastField<FStructProperty>(ColumnProperty))
	{
		if (StructProperty->Struct == TBaseStructure<FLinearColor>::Get())
		{
			CurrentColumn = MutableTable->FindColumn(ColumnName);

			if (CurrentColumn == INDEX_NONE)
			{
				CurrentColumn = MutableTable->AddColumn(ColumnName, mu::ETableColumnType::Color);
			}

			// Setting cell value
			FLinearColor Value = *(FLinearColor*)CellData;
			MutableTable->SetCell(CurrentColumn, RowIdx, Value);
		}
		
		else
		{
			// Unsuported Variable Type
			return false;
		}
	}

	else if (const FNumericProperty* FloatNumProperty = CastField<FFloatProperty>(ColumnProperty))
	{
		CurrentColumn = MutableTable->FindColumn(ColumnName);

		if (CurrentColumn == INDEX_NONE)
		{
			CurrentColumn = MutableTable->AddColumn(ColumnName, mu::ETableColumnType::Scalar);
		}

		// Setting cell value
		float Value = FloatNumProperty->GetFloatingPointPropertyValue(CellData);
		MutableTable->SetCell(CurrentColumn, RowIdx, Value);
	}

	else if (const FNumericProperty* DoubleNumProperty = CastField<FDoubleProperty>(ColumnProperty))
	{
		CurrentColumn = MutableTable->FindColumn(ColumnName);
	
		if (CurrentColumn == INDEX_NONE)
		{
			CurrentColumn = MutableTable->AddColumn(ColumnName, mu::ETableColumnType::Scalar);
		}
	
		// Setting cell value
		float Value = DoubleNumProperty->GetFloatingPointPropertyValue(CellData);
		MutableTable->SetCell(CurrentColumn, RowIdx, Value);
	}

	else
	{
		// Unsuported Variable Type
		return false;
	}

	return true;
}


bool GenerateTableColumn(const UCustomizableObjectNodeTable* TableNode, const UEdGraphPin* Pin, mu::TablePtr MutableTable, const FString& DataTableColumnName, const FProperty* ColumnProperty,
	const int32 LODIndexConnected, const int32 SectionIndexConnected, const int32 LODIndex, const int32 SectionIndex, const bool bOnlyConnectedLOD, FMutableGraphGenerationContext& GenerationContext)
{
	SCOPED_PIN_DATA(GenerationContext, Pin)

	if (!TableNode || !TableNode->Table || !TableNode->Table->GetRowStruct())
	{
		return false;
	}
	
	GenerationContext.AddParticipatingObject(*TableNode->Table);

	// Getting names of the rows to access the information
	TArray<FName> RowNames = TableNode->GetRowNames();

	for (int32 RowIndex = 0; RowIndex < RowNames.Num(); ++RowIndex)
	{
		// Getting Row Data
		uint8* RowData = TableNode->Table->FindRowUnchecked(RowNames[RowIndex]);

		if (RowData)
		{
			// Getting Cell Data
			uint8* CellData = ColumnProperty->ContainerPtrToValuePtr<uint8>(RowData, 0);

			if (CellData)
			{
				bool bCellGenerated = FillTableColumn(TableNode, MutableTable, DataTableColumnName, RowNames[RowIndex].ToString(), RowIndex, CellData, ColumnProperty,
					LODIndexConnected, SectionIndexConnected, LODIndex, SectionIndex, bOnlyConnectedLOD, GenerationContext);
				
				if (!bCellGenerated)
				{
					return false;
				}
			}
		}
	}

	return true;
}


mu::TablePtr GenerateMutableSourceTable(const FString& TableName, const UEdGraphPin* Pin, FMutableGraphGenerationContext& GenerationContext)
{	
	if (mu::TablePtr* Result = GenerationContext.GeneratedTables.Find(TableName))
	{
		return *Result;
	}

	mu::TablePtr MutableTable = new mu::Table();
	
	UCustomizableObjectNode* Node = CastChecked<UCustomizableObjectNode>(Pin->GetOwningNode());
	if (Node->IsNodeOutDatedAndNeedsRefresh())
	{
		Node->SetRefreshNodeWarning();
	}

	if (const UCustomizableObjectNodeTable* TypedTable = Cast<UCustomizableObjectNodeTable>(Node))
	{
		UDataTable* Table = TypedTable->Table;

		if (!Table)
		{
			FString msg = "Couldn't find the Data Table asset in the Node.";
			GenerationContext.Compiler->CompilerLog(FText::FromString(msg), Node);

			return nullptr;
		}

		const UScriptStruct* TableStruct = Table->GetRowStruct();

		if (TableStruct)
		{
			// Getting names of the rows to access the information
			TArray<FName> RowNames = TypedTable->GetRowNames();

			// Adding and filling Name Column
			MutableTable->AddColumn("Name", mu::ETableColumnType::String);

			// Add metadata
			FParameterUIData ParameterUIData(
				TypedTable->ParameterName,
				TypedTable->ParamUIMetadata,
				EMutableParameterType::Int);
			
			ParameterUIData.IntegerParameterGroupType = TypedTable->bAddNoneOption ? ECustomizableObjectGroupType::COGT_ONE_OR_NONE : ECustomizableObjectGroupType::COGT_ONE;

			// Adding a None Option
			MutableTable->SetNoneOption(TypedTable->bAddNoneOption);

			for (int32 i = 0; i < RowNames.Num(); ++i)
			{
				MutableTable->AddRow(i);
				FString RowName= RowNames[i].ToString();
				MutableTable->SetCell(0, i, RowName);
				ParameterUIData.ArrayIntegerParameterOption.Add(FIntegerParameterUIData(
					RowName,
					FMutableParamUIMetadata()));
			}

			GenerationContext.ParameterUIDataMap.Add(TypedTable->ParameterName, ParameterUIData);
		}
		else
		{
			FString msg = "Couldn't find the Data Table's Struct asset in the Node.";
			GenerationContext.Compiler->CompilerLog(FText::FromString(msg), Node);
			
			return nullptr;
		}
	}
	else
	{
		GenerationContext.Compiler->CompilerLog(LOCTEXT("UnimplementedNode", "Node type not implemented yet."), Node);

		return nullptr;
	}

	GenerationContext.GeneratedTables.Add(TableName, MutableTable);

	return MutableTable;
}

#undef LOCTEXT_NAMESPACE

