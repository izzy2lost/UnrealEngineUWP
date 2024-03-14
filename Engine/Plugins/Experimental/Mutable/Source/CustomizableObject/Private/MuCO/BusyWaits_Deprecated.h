// Copyright Epic Games, Inc. All Rights Reserved.

// ---------------------------------------------------------------
// THIS FILE SHOULD ONLY CONTAIN CODE BEING DEPRECATED BY UE-207754
// ---------------------------------------------------------------

#include "Templates/SharedPointer.h"

class FUpdateContextPrivate;
struct FMutableImageOperationData;
namespace mu
{
	class Model;
}


namespace impl_deprecated
{
	// This runs in the mutable thread.
	void Subtask_Mutable_GetImages(const TSharedRef<FUpdateContextPrivate>& OperationData);

	// This runs in a worker thread.
	void Task_Mutable_Update_GetImages(const TSharedRef<FUpdateContextPrivate>& OperationData);

	// This runs in the mutable thread.
	void Subtask_Mutable_BeginUpdate_GetMesh(const TSharedRef<FUpdateContextPrivate>& OperationData, TSharedPtr<mu::Model> Model);

	// This runs in a worker thread.
	void Task_Mutable_Update_GetMesh(const TSharedRef<FUpdateContextPrivate>& OperationData, const TSharedPtr<mu::Model>& Model);

}

namespace CustomizableObjectMipDataProvider::ImplDeprecated
{
	// This runs in the mutable thread.
	void Task_Mutable_UpdateImage(TSharedPtr<FMutableImageOperationData> OperationData);
}
