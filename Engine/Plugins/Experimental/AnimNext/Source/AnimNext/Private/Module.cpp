// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "AnimNextConfig.h"
#include "Animation/BlendProfile.h"
#include "Curves/CurveFloat.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Misc/CoreDelegates.h"
#include "DataRegistry.h"
#include "DecoratorBase/DecoratorRegistry.h"
#include "DecoratorBase/NodeTemplateRegistry.h"
#include "RigVMCore/RigVMRegistry.h"
#include "RigVMRuntimeDataRegistry.h"
#include "Animation/AnimSequence.h"
#include "Scheduler/Scheduler.h"
#include "Param/ExternalParameterRegistry.h"
#include "Param/ObjectProxyFactory.h"

namespace UE::AnimNext
{

class FModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		GetMutableDefault<UAnimNextConfig>()->LoadConfig();

		static TPair<UClass*, FRigVMRegistry::ERegisterObjectOperation> const AllowedObjectTypes[] =
		{
			{ UAnimSequence::StaticClass(), FRigVMRegistry::ERegisterObjectOperation::Class },
			{ UScriptStruct::StaticClass(), FRigVMRegistry::ERegisterObjectOperation::Class },
			{ UBlendProfile::StaticClass(), FRigVMRegistry::ERegisterObjectOperation::Class },
			{ UCurveFloat::StaticClass(), FRigVMRegistry::ERegisterObjectOperation::Class },
		};

		FRigVMRegistry::Get().RegisterObjectTypes(AllowedObjectTypes);

		FObjectProxyFactory::Init();
		FExternalParameterRegistry::Init();
		FDataRegistry::Init();
		FDecoratorRegistry::Init();
		FNodeTemplateRegistry::Init();
		FScheduler::Init();
		FRigVMRuntimeDataRegistry::Init();
	}

	virtual void ShutdownModule() override
	{
		FRigVMRuntimeDataRegistry::Destroy();
		FScheduler::Destroy();
		FNodeTemplateRegistry::Destroy();
		FDecoratorRegistry::Destroy();
		FDataRegistry::Destroy();
		FObjectProxyFactory::Destroy();
		FExternalParameterRegistry::Destroy();
	}
};

}

IMPLEMENT_MODULE(UE::AnimNext::FModule, AnimNext)
