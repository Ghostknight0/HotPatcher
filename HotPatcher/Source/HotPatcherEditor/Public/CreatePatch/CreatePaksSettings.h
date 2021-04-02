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

/** Singleton wrapper to allow for using the setting structure in SSettingsView */
UCLASS()
class UCreatePaksSettings : public UObject
{
	GENERATED_BODY()
public:
	UCreatePaksSettings()
	{
		AssetRegistryDependencyTypes.Add(EAssetRegistryDependencyTypeEx::Packages);
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SaveTo")
		FDirectoryPath SavePath;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pak Options")
		FDirectoryPath BuildDataPath;

	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pak Options")
	//	FDirectoryPath PakListsPath;

};
