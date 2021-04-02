#pragma once

#include "ETargetPlatform.h"

// engine header
#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "FDLCFeatureConfig.generated.h"


USTRUCT(BlueprintType)
struct FDLCFeatureConfig
{
	GENERATED_USTRUCT_BODY()
public:

	FDLCFeatureConfig()
	{

	}

	bool operator==(const FDLCFeatureConfig& InAsset)const
	{
		bool SameSceneName = (SceneName == InAsset.SceneName);
		bool SameSceneType = (SceneType == InAsset.SceneType);
		bool SameScenePriority = (ScenePriority == InAsset.ScenePriority);

		return SameSceneName && SameSceneType && SameScenePriority;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString SceneName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString SceneType;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ScenePriority;
};
