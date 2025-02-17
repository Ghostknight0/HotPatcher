﻿#pragma once

#include "ETargetPlatform.h"
#include "FExternFileInfo.h"
#include "FExternDirectoryInfo.h"

// engine heacer
#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "FPlatformExternAssets.generated.h"


USTRUCT(BlueprintType)
struct FPlatformExternAssets
{
    GENERATED_USTRUCT_BODY()

    UPROPERTY(EditAnywhere,BlueprintReadWrite)
    ETargetPlatform TargetPlatform;

    UPROPERTY(EditAnywhere,BlueprintReadWrite)
    TArray<FExternFileInfo> AddExternFileToPak;
    UPROPERTY(EditAnywhere,BlueprintReadWrite)
    TArray<FExternDirectoryInfo> AddExternDirectoryToPak;


    bool operator==(const FPlatformExternAssets& R)const
    {
        return TargetPlatform == R.TargetPlatform;
    }
};

USTRUCT(BlueprintType)
struct FPlatformExternAssetsEx :public FPlatformExternAssets
{
    GENERATED_USTRUCT_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString PakName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString SceneType = TEXT("Apk");

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIteratePak = false;
};
