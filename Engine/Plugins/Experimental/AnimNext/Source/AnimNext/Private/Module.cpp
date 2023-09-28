// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Misc/CoreDelegates.h"
#include "DataRegistry.h"
#include "DecoratorBase/DecoratorRegistry.h"
#include "DecoratorBase/NodeTemplateRegistry.h"
#include "RigVMCore/RigVMRegistry.h"
#include "Animation/AnimSequence.h"

namespace UE::AnimNext
{

class FModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		static TPair<UClass*, FRigVMRegistry::ERegisterObjectOperation> const AllowedObjectTypes[] =
		{
			{ UAnimSequence::StaticClass(), FRigVMRegistry::ERegisterObjectOperation::Class },
			{ UScriptStruct::StaticClass(), FRigVMRegistry::ERegisterObjectOperation::Class }
		};

		FRigVMRegistry::Get().RegisterObjectTypes(AllowedObjectTypes);

		FDataRegistry::Init();
		FDecoratorRegistry::Init();
		FNodeTemplateRegistry::Init();
	}

	virtual void ShutdownModule() override
	{
		FNodeTemplateRegistry::Destroy();
		FDecoratorRegistry::Destroy();
		FDataRegistry::Destroy();
	}
};

}

IMPLEMENT_MODULE(UE::AnimNext::FModule, AnimNext)
