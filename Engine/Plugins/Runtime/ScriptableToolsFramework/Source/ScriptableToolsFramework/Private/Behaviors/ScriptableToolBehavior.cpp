#include "ScriptableToolBehavior.h"

void UScriptableToolBehavior::SetDefaultPriority(const FInputCapturePriority& Priority)
{
	GetWrappedBehavior()->SetDefaultPriority(Priority);
}


