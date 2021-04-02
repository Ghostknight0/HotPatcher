#pragma once

#include "Commandlets/Commandlet.h"
#include "CreatePakCommandlet.generated.h"


UCLASS()
class UCreatePakCommandlet :public UCommandlet
{
	GENERATED_BODY()

public:

	virtual int32 Main(const FString& Params)override;
};
