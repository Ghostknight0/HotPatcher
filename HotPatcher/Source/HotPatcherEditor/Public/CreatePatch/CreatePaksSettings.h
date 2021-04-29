// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

// project header
#include "FPatcherSpecifyAsset.h"
#include "FDLCFeatureConfig.h"

#include "FPatchVersionDiff.h"
#include "FChunkInfo.h"
#include "FReplaceText.h"
#include "ETargetPlatform.h"
#include "FExternFileInfo.h"
#include "FExternDirectoryInfo.h"
#include "FPatcherSpecifyAsset.h"
#include "FlibPatchParserHelper.h"
#include "FlibHotPatcherEditorHelper.h"

// engine header
#include "Engine/EngineTypes.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

#include "Misc/FileHelper.h"
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"
#include "CreatePaksSettings.generated.h"

USTRUCT(BlueprintType)
struct FINTMap
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PakSize)
	TMap<int32, int32> PakSize;

	FINTMap()
	{}

	FINTMap(int32 paknum,int32 pakspace)
	{
		PakSize.Add(paknum, pakspace);
	}
};

/** Singleton wrapper to allow for using the setting structure in SSettingsView */
UCLASS()
class UCreatePaksSettings : public UObject
{
	GENERATED_BODY()
public:
	UCreatePaksSettings()
	{
		AssetRegistryDependencyTypes.Add(EAssetRegistryDependencyTypeEx::Packages);
		TArray<FString> InFilterPackagePaths = TArray<FString>{ "/Game/UI", "/Game/Effects", "/Game/Characters" };
		for (const auto& Filter: InFilterPackagePaths)
		{
			FDirectoryPath DirectoryPath;
			DirectoryPath.Path = Filter;
			AssetMandatoryIncludeFilters.Add(DirectoryPath);
		}
		
		PakSizeControl = {
			{"Default",					FINTMap(50,1024 * 1024 * 10)},
			{"Font",					FINTMap(30,1024 * 1024 * 10)},
			{"PrimaryAssetLabel",		FINTMap(1 ,1024 * 1024 * 10)},
			{"World",					FINTMap(1 ,1024 * 1024 * 10)},
			{"SkeletalMesh",			FINTMap(20,1024 * 1024 * 10)},
			{"StaticMesh",				FINTMap(30,1024 * 1024 * 10)},
			{"ParticleSystem",			FINTMap(20,1024 * 1024 * 10)},
			{"Texture2D",				FINTMap(30,1024 * 1024 * 10)},
			{"Material",				FINTMap(20,1024 * 1024 * 10)},
			{"MaterialFunction",		FINTMap(20,1024 * 1024 * 10)},
			{"PhysicalMaterial",		FINTMap(20,1024 * 1024 * 10)},
			{"MaterialInstanceConstant",FINTMap(20,1024 * 1024 * 10)},
			{"BlendSpace",				FINTMap(20,1024 * 1024 * 10)},
			{"SoundCue",				FINTMap(30,1024 * 1024 * 10)},
			{"SoundWave",				FINTMap(20,1024 * 1024 * 10)},
			{"SoundAttenuation",		FINTMap(20,1024 * 1024 * 10)},
			{"SoundClass",				FINTMap(20,1024 * 1024 * 10)},
		};

	}

	FORCEINLINE FString GetLogsFilePath()const
	{
		return LogsFilePath.Path;
	}

	//FORCEINLINE FString GetPakListsPath()const
	//{
	//	return PakListsPath.Path;
	//}

	FORCEINLINE TArray<ETargetPlatform> GetPakTargetPlatforms()const
	{
		return PakTargetPlatforms;
	}

	FORCEINLINE TArray<EAssetRegistryDependencyTypeEx> GetAssetRegistryDependencyTypes()const
	{
		return AssetRegistryDependencyTypes;
	}

	FORCEINLINE TArray<FDLCFeatureConfig> GetFeatureConfigs()const
	{
		return FeatureConfigs;
	}
	FORCEINLINE FString GetSavePath()const
	{
		return SavePath.Path;
	}
	FORCEINLINE TArray<FPlatformExternAssetsEx> GetAddExternAssetsToPlatform()const
	{
		return AddExternAssetsToPlatform;
	}

	FORCEINLINE TArray<FDirectoryPath> GetAssetMandatoryIncludeFilters()const
	{
		return AssetMandatoryIncludeFilters;
	}

	FORCEINLINE TMap<FString, FINTMap> GetPakSizeControl()const
	{
		return PakSizeControl;
	}

	FORCEINLINE static UCreatePaksSettings* Get()
	{
		static bool bInitialized = false;
		// This is a singleton, use default object
		UCreatePaksSettings* DefaultSettings = GetMutableDefault<UCreatePaksSettings>();
		DefaultSettings->AddToRoot();
		if (!bInitialized)
		{
			bInitialized = true;
		}

		return DefaultSettings;
	}

	FORCEINLINE bool SerializeCreatePaksConfigToString(FString& OutJsonString)
	{
		TSharedPtr<FJsonObject> ReleaseConfigJsonObject = MakeShareable(new FJsonObject);
		SerializeCreatePaksConfigToJsonObject(ReleaseConfigJsonObject);

		auto JsonWriter = TJsonWriterFactory<TCHAR>::Create(&OutJsonString);
		return FJsonSerializer::Serialize(ReleaseConfigJsonObject.ToSharedRef(), JsonWriter);
	}

	FORCEINLINE bool SerializeCreatePaksConfigToJsonObject(TSharedPtr<FJsonObject>& OutJsonObject)
	{
		return UFlibHotPatcherEditorHelper::SerializeCreatePaksConfigToJsonObject(this, OutJsonObject);
	}

	FORCEINLINE FString GetBuildDataPath()const { return BuildDataPath.Path; }

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PatchBaseSettings")
		FString VersionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pak Options")
		TArray<ETargetPlatform> PakTargetPlatforms;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pak Options")
		TArray<EAssetRegistryDependencyTypeEx> AssetRegistryDependencyTypes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pak Options")
		FDirectoryPath LogsFilePath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pak Options")
		TArray<FDLCFeatureConfig> FeatureConfigs;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pak Options")
		TMap<FString, FINTMap> PakSizeControl;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extern Files")
		TArray<FPlatformExternAssetsEx> AddExternAssetsToPlatform;

	//强制预先处理的打包路径
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asset Filter", meta = (RelativeToGameContentDir, LongPackageName))
		TArray<FDirectoryPath> AssetMandatoryIncludeFilters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SaveTo")
		FDirectoryPath SavePath;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pak Options")
		FDirectoryPath BuildDataPath;

	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pak Options")
	//	FDirectoryPath PakListsPath;

};

