#pragma once 

#include "HotPatcherLog.h"
#include "CreatePatch/HotPatcherSettingBase.h"
#include "FlibPatchParserHelper.h"
#include "HotPatcherLog.h"
#include "ETargetPlatform.h"

// engine header
#include "Misc/FileHelper.h"
#include "CoreMinimal.h"

#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"
#include "Kismet/KismetStringLibrary.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include "FExportShaderPatchSettings.generated.h"


USTRUCT(BlueprintType)
struct FShaderPatchConf
{
	GENERATED_USTRUCT_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		ETargetPlatform Platform;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
    	TArray<FDirectoryPath> OldMetadataDir;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    	FDirectoryPath NewMetadataDir;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
   		bool bNativeFormat;
	// since UE 4.26 (Below 4.26 this parameter has no effect)
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
		bool bDeterministicShaderCodeOrder = true;
};

/** Singleton wrapper to allow for using the setting structure in SSettingsView */
USTRUCT(BlueprintType)
struct HOTPATCHEREDITOR_API FExportShaderPatchSettings:public FHotPatcherSettingBase
{
	GENERATED_USTRUCT_BODY()
public:
	FExportShaderPatchSettings(){}
	virtual ~FExportShaderPatchSettings(){};

	FORCEINLINE static FExportShaderPatchSettings* Get()
	{
		static FExportShaderPatchSettings StaticIns;

		return &StaticIns;
	}
	
	UPROPERTY(EditAnywhere, Category="Version")
	FString VersionID;
	
	UPROPERTY(EditAnywhere, Category="Config")
	TArray<FShaderPatchConf> ShaderPatchConfigs;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	bool bSaveConfig = true;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Save")
	FDirectoryPath SaveTo;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Advanced")
	bool bStandaloneMode = false;
};


