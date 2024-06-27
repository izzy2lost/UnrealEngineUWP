// Copyright Epic Games, Inc. All Rights Reserved.

#include "Expressions/Input/TG_Expression_TexturePath.h"

#include "TG_Graph.h"
#include "2D/TextureHelper.h"
#include "Model/StaticImageResource.h"

void UTG_Expression_TexturePath::Evaluate(FTG_EvaluationContext* InContext)
{
	Super::Evaluate(InContext);
	if (Texture)
	{
		Output = Texture;
	}
	else if (!Path.IsEmpty())
	{
		UStaticImageResource* StaticImageResource = UStaticImageResource::CreateNew<UStaticImageResource>();

		FString LocalPath = Path;
		FPackagePath PackagePath;
		FString PathExt = FPaths::GetExtension(Path);

		if (FPackagePath::TryFromMountedName(Path, PackagePath))
		{
			LocalPath = PackagePath.GetLocalFullPath();
			FString LocalPathExt = FPaths::GetExtension(LocalPath);

			if (LocalPathExt != PathExt)
			{
				LocalPath = FPaths::ChangeExtension(LocalPath, PathExt);
			}
		}

		StaticImageResource->SetAssetUUID(LocalPath);
		StaticImageResource->SetIsFileSystem(true);

		//Until we have srgb value exposed in the UI we need to set the Srgb of the Output Descriptor here from the source
		//This gets updated for the late bond case but since we do not have the UI to specify the override in other nodes 
		// the override value will always be set to false while combining the buffers
		auto DesiredDesc = Output.GetBufferDescriptor();
		DesiredDesc.bIsSRGB = true;

		Output = StaticImageResource->GetBlob(InContext->Cycle, DesiredDesc, 0);
	}
	else
	{
		Output = FTG_Texture::GetBlack();
	}
}

bool UTG_Expression_TexturePath::Validate(MixUpdateCyclePtr Cycle)
{
	return true;
}
void UTG_Expression_TexturePath::SetTitleName(FName NewName)
{
	GetParentNode()->GetPin(GET_MEMBER_NAME_CHECKED(UTG_Expression_TexturePath, Path))->SetAliasName(NewName);
}

FName UTG_Expression_TexturePath::GetTitleName() const
{
	return GetParentNode()->GetPin(GET_MEMBER_NAME_CHECKED(UTG_Expression_TexturePath, Path))->GetAliasName();
}

