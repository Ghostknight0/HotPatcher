// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "GeneratedFileCrcCommandlet.generated.h"

/**
 * 
 */
UCLASS()
class KDLC_API UGeneratedFileCrcCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	virtual int32 Main(const FString& Params)override;
};
