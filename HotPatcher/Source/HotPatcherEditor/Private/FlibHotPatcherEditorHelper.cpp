// Fill out your copyright notice in the Description page of Project Settings.


#include "FlibHotPatcherEditorHelper.h"
#include "CreatePatch/FExportPatchSettings.h"
#include "CreatePatch/FExportReleaseSettings.h"
#include "HotPatcherLog.h"
#include <HotPatcherEditor/Public/CreatePatch/CreatePaksSettings.h>

// engine header
#include "IPlatformFileSandboxWrapper.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/SecureHash.h"

DEFINE_LOG_CATEGORY(LogHotPatcherEditorHelper);

TArray<FString> UFlibHotPatcherEditorHelper::GetAllCookOption()
{
	TArray<FString> result
	{
		"Iterate",
		"UnVersioned",
		"CookAll",
		"Compressed"
	};
	return result;
}

void UFlibHotPatcherEditorHelper::CreateSaveFileNotify(const FText& InMsg, const FString& InSavedFile,SNotificationItem::ECompletionState NotifyType)
{
	auto Message = InMsg;
	FNotificationInfo Info(Message);
	Info.bFireAndForget = true;
	Info.ExpireDuration = 5.0f;
	Info.bUseSuccessFailIcons = false;
	Info.bUseLargeFont = false;

	const FString HyperLinkText = InSavedFile;
	Info.Hyperlink = FSimpleDelegate::CreateLambda(
		[](FString SourceFilePath)
		{
			FPlatformProcess::ExploreFolder(*SourceFilePath);
		},
		HyperLinkText
	);
	Info.HyperlinkText = FText::FromString(HyperLinkText);

	FSlateNotificationManager::Get().AddNotification(Info)->SetCompletionState(NotifyType);
}

void UFlibHotPatcherEditorHelper::CheckInvalidCookFilesByAssetDependenciesInfo(
	const FString& InProjectAbsDir, 
	const FString& InPlatformName, 
	const FAssetDependenciesInfo& InAssetDependencies, 
	TArray<FAssetDetail>& OutValidAssets, 
	TArray<FAssetDetail>& OutInvalidAssets)
{
	OutValidAssets.Empty();
	OutInvalidAssets.Empty();
	TArray<FAssetDetail> AllAssetDetails;
	UFLibAssetManageHelperEx::GetAssetDetailsByAssetDependenciesInfo(InAssetDependencies,AllAssetDetails);

	for (const auto& AssetDetail : AllAssetDetails)
	{
		TArray<FString> CookedAssetPath;
		TArray<FString> CookedAssetRelativePath;
		FString AssetLongPackageName;
		UFLibAssetManageHelperEx::ConvPackagePathToLongPackageName(AssetDetail.mPackagePath, AssetLongPackageName);
		if (UFLibAssetManageHelperEx::ConvLongPackageNameToCookedPath(
			InProjectAbsDir,
			InPlatformName,
			AssetLongPackageName,
			CookedAssetPath,
			CookedAssetRelativePath))
		{
			if (CookedAssetPath.Num() > 0)
			{
				OutValidAssets.Add(AssetDetail);
			}
			else
			{
				OutInvalidAssets.Add(AssetDetail);
			}
		}
	}
}


#include "Kismet/KismetSystemLibrary.h"

FChunkInfo UFlibHotPatcherEditorHelper::MakeChunkFromPatchSettings(const FExportPatchSettings* InPatchSetting)
{
	FChunkInfo Chunk;
	if (!(InPatchSetting && InPatchSetting))
	{
		return Chunk;
	}
	
	Chunk.ChunkName = InPatchSetting->VersionId;
	Chunk.bMonolithic = false;
	Chunk.MonolithicPathMode = EMonolithicPathMode::MountPath;
	Chunk.bSavePakCommands = true;
	Chunk.AssetIncludeFilters = const_cast<FExportPatchSettings*>(InPatchSetting)->GetAssetIncludeFilters();
	Chunk.AssetIgnoreFilters = const_cast<FExportPatchSettings*>(InPatchSetting)->GetAssetIgnoreFilters();
	Chunk.bAnalysisFilterDependencies = InPatchSetting->IsAnalysisFilterDependencies();
	Chunk.IncludeSpecifyAssets = const_cast<FExportPatchSettings*>(InPatchSetting)->GetIncludeSpecifyAssets();
	Chunk.AddExternAssetsToPlatform = const_cast<FExportPatchSettings*>(InPatchSetting)->GetAddExternAssetsToPlatform();
	Chunk.AssetRegistryDependencyTypes = InPatchSetting->GetAssetRegistryDependencyTypes();
	Chunk.InternalFiles.bIncludeAssetRegistry = InPatchSetting->IsIncludeAssetRegistry();
	Chunk.InternalFiles.bIncludeGlobalShaderCache = InPatchSetting->IsIncludeGlobalShaderCache();
	Chunk.InternalFiles.bIncludeShaderBytecode = InPatchSetting->IsIncludeShaderBytecode();
	Chunk.InternalFiles.bIncludeEngineIni = InPatchSetting->IsIncludeEngineIni();
	Chunk.InternalFiles.bIncludePluginIni = InPatchSetting->IsIncludePluginIni();
	Chunk.InternalFiles.bIncludeProjectIni = InPatchSetting->IsIncludeProjectIni();

	return Chunk;
}

FChunkInfo UFlibHotPatcherEditorHelper::MakeChunkFromPatchVerison(const FHotPatcherVersion& InPatchVersion)
{
	FChunkInfo Chunk;
	Chunk.ChunkName = InPatchVersion.VersionId;
	Chunk.bMonolithic = false;
	Chunk.bSavePakCommands = false;
	auto ConvPathStrToDirPaths = [](const TArray<FString>& InPathsStr)->TArray<FDirectoryPath>
	{
		TArray<FDirectoryPath> result;
		for (const auto& Dir : InPathsStr)
		{
			FDirectoryPath Path;
			Path.Path = Dir;
			result.Add(Path);
		}
		return result;
	};

	//Chunk.AssetIncludeFilters = ConvPathStrToDirPaths(InPatchVersion.IgnoreFilter);
	// Chunk.AssetIgnoreFilters = ConvPathStrToDirPaths(InPatchVersion.IgnoreFilter);
	Chunk.bAnalysisFilterDependencies = false;
	TArray<FAssetDetail> AllVersionAssets;
	UFLibAssetManageHelperEx::GetAssetDetailsByAssetDependenciesInfo(InPatchVersion.AssetInfo, AllVersionAssets);

	for (const auto& Asset : AllVersionAssets)
	{
		FPatcherSpecifyAsset CurrentAsset;
		CurrentAsset.Asset = FSoftObjectPath(Asset.mPackagePath);
		CurrentAsset.bAnalysisAssetDependencies = false;
		Chunk.IncludeSpecifyAssets.AddUnique(CurrentAsset);
	}
	// Chunk.AddExternDirectoryToPak = InPatchSetting->GetAddExternDirectory();
	// for (const auto& File : InPatchVersion.ExternalFiles)
	// {
	// 	Chunk.AddExternFileToPak.AddUnique(File.Value);
	// }

	TArray<ETargetPlatform> VersionPlatforms;

	InPatchVersion.PlatformAssets.GetKeys(VersionPlatforms);

	for(auto Platform:VersionPlatforms)
	{
		Chunk.AddExternAssetsToPlatform.Add(InPatchVersion.PlatformAssets[Platform]);
	}
	
	Chunk.InternalFiles.bIncludeAssetRegistry = false;
	Chunk.InternalFiles.bIncludeGlobalShaderCache = false;
	Chunk.InternalFiles.bIncludeShaderBytecode = false;
	Chunk.InternalFiles.bIncludeEngineIni = false;
	Chunk.InternalFiles.bIncludePluginIni = false;
	Chunk.InternalFiles.bIncludeProjectIni = false;

	return Chunk;
}

#define REMAPPED_PLUGGINS TEXT("RemappedPlugins")


FString ConvertToFullSandboxPath( const FString &FileName, bool bForWrite )
{
	FString ProjectContentAbsir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
	if(FileName.StartsWith(ProjectContentAbsir))
	{
		FString GameFileName = FileName;
		GameFileName.RemoveFromStart(ProjectContentAbsir);
		return FPaths::Combine(FApp::GetProjectName(),TEXT("Content"),GameFileName);
	}
	if(FileName.StartsWith(FPaths::EngineContentDir()))
	{
		FString EngineFileName = FileName;
		EngineFileName.RemoveFromStart(FPaths::EngineContentDir());
		return FPaths::Combine(TEXT("Engine/Content"),EngineFileName);;
	}
	TArray<TSharedRef<IPlugin> > PluginsToRemap = IPluginManager::Get().GetEnabledPlugins();
	// Ideally this would be in the Sandbox File but it can't access the project or plugin
	if (PluginsToRemap.Num() > 0)
	{
		// Handle remapping of plugins
		for (TSharedRef<IPlugin> Plugin : PluginsToRemap)
		{
			FString PluginContentDir;
			if (FPaths::IsRelative(FileName))
				PluginContentDir = Plugin->GetContentDir();
			else
				PluginContentDir = FPaths::ConvertRelativePathToFull(Plugin->GetContentDir());
			// UE_LOG(LogHotPatcherEditorHelper,Log,TEXT("Plugin Content:%s"),*PluginContentDir);
			if (FileName.StartsWith(PluginContentDir))
			{
				FString SearchFor;
				SearchFor /= Plugin->GetName() / TEXT("Content");
				int32 FoundAt = FileName.Find(SearchFor, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
				check(FoundAt != -1);
				// Strip off everything but <PluginName/Content/<remaing path to file>
				FString SnippedOffPath = FileName.RightChop(FoundAt);

				FString LoadingFrom;
				switch(Plugin->GetLoadedFrom())
				{
				case EPluginLoadedFrom::Engine:
					{
						LoadingFrom = TEXT("Engine/Plugins");
						break;
					}
				case EPluginLoadedFrom::Project:
					{
						LoadingFrom = FPaths::Combine(FApp::GetProjectName(),TEXT("Plugins"));
						break;
					}
				}
					
				return FPaths::Combine(LoadingFrom,SnippedOffPath);
			}
		}
	}

	return TEXT("");
}

FString UFlibHotPatcherEditorHelper::GetCookAssetsSaveDir(const FString& BaseDir, UPackage* Package, const FString& Platform)
{
	FString Filename;
	FString PackageFilename;
	FString StandardFilename;
	FName StandardFileFName = NAME_None;

	if (FPackageName::DoesPackageExist(Package->FileName.ToString(), NULL, &Filename, false))
	{
		StandardFilename = PackageFilename = FPaths::ConvertRelativePathToFull(Filename);

		FPaths::MakeStandardFilename(StandardFilename);
		StandardFileFName = FName(*StandardFilename);
	}

	FString SandboxFilename = ConvertToFullSandboxPath(*StandardFilename, true);
	// UE_LOG(LogHotPatcherEditorHelper,Log,TEXT("Filename:%s,PackageFileName:%s,StandardFileName:%s"),*Filename,*PackageFilename,*StandardFilename);
	
	FString CookDir =FPaths::Combine(BaseDir,Platform,SandboxFilename);
	
	return 	CookDir;
}

FString UFlibHotPatcherEditorHelper::GetProjectCookedDir()
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Cooked")));;
}



bool UFlibHotPatcherEditorHelper::CookAssets(const TArray<FSoftObjectPath>& Assets, const TArray<ETargetPlatform>&Platforms,
                                            const FString& SavePath)
{
	FString FinalSavePath = SavePath;
	if(FinalSavePath.IsEmpty())
	{
		FinalSavePath = UFlibHotPatcherEditorHelper::GetProjectCookedDir();
	}
	TArray<FAssetData> AssetsData;
	TArray<UPackage*> Packages;
	for(const auto& Asset:Assets)
	{
		FAssetData AssetData;
		if(UFLibAssetManageHelperEx::GetSingleAssetsData(Asset.GetAssetPathString(),AssetData))
		{
			AssetsData.AddUnique(AssetData);
			Packages.AddUnique(AssetData.GetPackage());
		}
	}
	TArray<FString> StringPlatforms;
	for(const auto& Platform:Platforms)
	{
		StringPlatforms.AddUnique(UFlibPatchParserHelper::GetEnumNameByValue(Platform));
	}
	
	return CookPackages(Packages,StringPlatforms,SavePath);
}

bool UFlibHotPatcherEditorHelper::CookPackage(UPackage* Package, const TArray<FString>& Platforms, const FString& SavePath)
{
	bool bSuccessed = false;
	const bool bSaveConcurrent = FParse::Param(FCommandLine::Get(), TEXT("ConcurrentSave"));
	bool bUnversioned = false;
	uint32 SaveFlags = SAVE_KeepGUID | SAVE_Async | SAVE_ComputeHash | (bUnversioned ? SAVE_Unversioned : 0);
	EObjectFlags CookedFlags = RF_Public;
	if(Cast<UWorld>(Package))
	{
		CookedFlags = RF_NoFlags;
	}
	if (bSaveConcurrent)
	{
		SaveFlags |= SAVE_Concurrent;
	}
	ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();
	const TArray<ITargetPlatform*>& TargetPlatforms = TPM.GetTargetPlatforms();
	TArray<ITargetPlatform*> CookPlatforms; 
	for (ITargetPlatform *TargetPlatform : TargetPlatforms)
	{
		if (Platforms.Contains(TargetPlatform->PlatformName()))
		{
			CookPlatforms.AddUnique(TargetPlatform);
		}
	}
	if(Package->FileName.IsNone())
		return bSuccessed;
	for(auto& Platform:CookPlatforms)
	{
		struct FFilterEditorOnlyFlag
		{
			FFilterEditorOnlyFlag(UPackage* InPackage,ITargetPlatform* InPlatform)
			{
				Package = InPackage;
				Platform = InPlatform;
				if(!Platform->HasEditorOnlyData())
				{
					Package->SetPackageFlags(PKG_FilterEditorOnly);
				}
				else
				{
					Package->ClearPackageFlags(PKG_FilterEditorOnly);
				}
			}
			~FFilterEditorOnlyFlag()
			{
				if(!Platform->HasEditorOnlyData())
				{
					Package->ClearPackageFlags(PKG_FilterEditorOnly);
				}
			}
			UPackage* Package;
			ITargetPlatform* Platform;
		};

		FFilterEditorOnlyFlag SetPackageEditorOnlyFlag(Package,Platform);
		
		FString CookedSavePath = UFlibHotPatcherEditorHelper::GetCookAssetsSaveDir(SavePath,Package, Platform->PlatformName());
		// delete old cooked assets
		if(FPaths::FileExists(CookedSavePath))
		{
			IFileManager::Get().Delete(*CookedSavePath);
		}
		// UE_LOG(LogHotPatcherEditorHelper,Log,TEXT("Cook Assets:%s"),*Package->GetName());
		Package->FullyLoad();
		TArray<UObject*> ExportMap;
		GetObjectsWithOuter(Package,ExportMap);
		for(const auto& ExportObj:ExportMap)
		{
			ExportObj->BeginCacheForCookedPlatformData(Platform);
		}

		// if(!bSaveConcurrent)
		// {
		// 	TArray<UObject*> TagExpObjects;
		// 	GetObjectsWithAnyMarks(TagExpObjects,OBJECTMARK_TagExp);
		// 	for(const auto& TagExportObj:TagExpObjects)
		// 	{
		// 		if(TagExportObj->HasAnyMarks(OBJECTMARK_TagExp))
		// 		{
		// 			TagExportObj->BeginCacheForCookedPlatformData(Platform);
		// 		}
		// 	}
		// }
		
		GIsCookerLoadingPackage = true;
		UE_LOG(LogHotPatcherEditorHelper,Display,TEXT("Cook Assets:%s save to %s"),*Package->GetName(),*CookedSavePath);
		FSavePackageResultStruct Result = GEditor->Save(	Package, nullptr, CookedFlags, *CookedSavePath, 
                                                GError, nullptr, false, false, SaveFlags, Platform, 
                                                FDateTime::MinValue(), false, /*DiffMap*/ nullptr);
		GIsCookerLoadingPackage = false;
		bSuccessed = Result == ESavePackageResult::Success;
	}
	return bSuccessed;
}

ITargetPlatform* UFlibHotPatcherEditorHelper::GetTargetPlatformByName(const FString& PlatformName)
{
	ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();
	const TArray<ITargetPlatform*>& TargetPlatforms = TPM.GetTargetPlatforms();
	ITargetPlatform* PlatformIns = NULL; 
	for (ITargetPlatform *TargetPlatform : TargetPlatforms)
	{
		if (PlatformName.Contains(TargetPlatform->PlatformName()))
		{
			PlatformIns = TargetPlatform;
		}
	}
	return PlatformIns;
}

TArray<ITargetPlatform*> UFlibHotPatcherEditorHelper::GetTargetPlatformsByNames(const TArray<ETargetPlatform>& Platforms)
{

	TArray<ITargetPlatform*> result;
	for(const auto& Platform:Platforms)
	{
    			
		ITargetPlatform* Found = UFlibHotPatcherEditorHelper::GetTargetPlatformByName(UFlibPatchParserHelper::GetEnumNameByValue(Platform,false));
		if(Found)
		{
			result.Add(Found);
		}
	}
	return result;
}

bool UFlibHotPatcherEditorHelper::CookPackages(TArray<UPackage*>& InPackage, const TArray<FString>& Platforms, const FString& SavePath)
{
	for(const auto& Package:InPackage)
	{
		CookPackage(Package,Platforms,SavePath);
	}
	return true;
}



FString UFlibHotPatcherEditorHelper::GetUnrealPakBinary()
{
#if PLATFORM_WINDOWS
	return FPaths::Combine(
        FPaths::ConvertRelativePathToFull(FPaths::EngineDir()),
        TEXT("Binaries"),
#if PLATFORM_64BITS	
        TEXT("Win64"),
#else
        TEXT("Win32"),
#endif
        TEXT("UnrealPak.exe")
    );
#endif

#if PLATFORM_MAC
	return FPaths::Combine(
            FPaths::ConvertRelativePathToFull(FPaths::EngineDir()),
            TEXT("Binaries"),
            TEXT("Mac"),
            TEXT("UnrealPak")
    );
#endif

	return TEXT("");
}

FString UFlibHotPatcherEditorHelper::GetUE4CmdBinary()
{
#if PLATFORM_WINDOWS
	return FPaths::Combine(
        FPaths::ConvertRelativePathToFull(FPaths::EngineDir()),
        TEXT("Binaries"),
#if PLATFORM_64BITS	
        TEXT("Win64"),
#else
        TEXT("Win32"),
#endif
#ifdef WITH_HOTPATCHER_DEBUGGAME
	#if PLATFORM_64BITS
	        TEXT("UE4Editor-Win64-DebugGame-Cmd.exe")
	#else
	        TEXT("UE4Editor-Win32-DebugGame-Cmd.exe")
	#endif
#else
        TEXT("UE4Editor-Cmd.exe")
#endif
    );
#endif
#if PLATFORM_MAC
	return FPaths::Combine(
        FPaths::ConvertRelativePathToFull(FPaths::EngineDir()),
        TEXT("Binaries"),
        TEXT("Mac"),
        TEXT("UE4Editor-Cmd")
    );
#endif
	return TEXT("");
}


FProcHandle UFlibHotPatcherEditorHelper::DoUnrealPak(TArray<FString> UnrealPakOptions, bool block)
{
	FString UnrealPakBinary = UFlibHotPatcherEditorHelper::GetUnrealPakBinary();

	FString CommandLine;
	for (const auto& Option : UnrealPakOptions)
	{
		CommandLine.Append(FString::Printf(TEXT(" %s"), *Option));
	}

	// create UnrealPak process

	uint32 *ProcessID = NULL;
	FProcHandle ProcHandle = FPlatformProcess::CreateProc(*UnrealPakBinary, *CommandLine, true, false, false, ProcessID, 0, NULL, NULL, NULL);

	if (ProcHandle.IsValid())
	{
		if (block)
		{
			FPlatformProcess::WaitForProc(ProcHandle);
		}
	}
	return ProcHandle;
}

FString UFlibHotPatcherEditorHelper::GetMetadataDir(const FString& ProjectDir, const FString& ProjectName,ETargetPlatform Platform)
{
	FString result;
	FString PlatformName = UFlibPatchParserHelper::GetEnumNameByValue(Platform,false);
	return FPaths::Combine(ProjectDir,TEXT("Saved/Cooked"),PlatformName,ProjectName,TEXT("Metadata"));
}

void UFlibHotPatcherEditorHelper::BackupMetadataDir(const FString& ProjectDir, const FString& ProjectName,
	const TArray<ETargetPlatform>& Platforms, const FString& OutDir)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	for(const auto& Platform:Platforms)
	{
		FString MetadataDir = FPaths::ConvertRelativePathToFull(UFlibHotPatcherEditorHelper::GetMetadataDir(ProjectDir,ProjectName,Platform));
		FString OutMetadir = FPaths::Combine(OutDir,TEXT("BackupMatedatas"),UFlibPatchParserHelper::GetEnumNameByValue(Platform,false));
		if(FPaths::DirectoryExists(MetadataDir))
		{
			PlatformFile.CreateDirectoryTree(*OutMetadir);
			PlatformFile.CopyDirectoryTree(*OutMetadir,*MetadataDir,true);
		}
	}
}

bool UFlibHotPatcherEditorHelper::SerializeCreatePaksConfigToJsonObject(const UCreatePaksSettings* const InCreatePaksSetting, TSharedPtr<FJsonObject>& OutJsonObject)
{

	auto SerializeArrayLambda = [&OutJsonObject](const TArray<FString>& InArray, const FString& InJsonArrayName)
	{
		TArray<TSharedPtr<FJsonValue>> ArrayJsonValueList;
		for (const auto& ArrayItem : InArray)
		{
			ArrayJsonValueList.Add(MakeShareable(new FJsonValueString(ArrayItem)));
		}
		OutJsonObject->SetArrayField(InJsonArrayName, ArrayJsonValueList);
	};

	auto SerializeAssetDependencyTypes = [&OutJsonObject](const FString& InName, const TArray<EAssetRegistryDependencyTypeEx>& InTypes)
	{
		TArray<TSharedPtr<FJsonValue>> TypesJsonValues;
		for (const auto& Type : InTypes)
		{
			TSharedPtr<FJsonValue> CurrentJsonValue = MakeShareable(new FJsonValueString(UFlibPatchParserHelper::GetEnumNameByValue(Type)));
			TypesJsonValues.Add(CurrentJsonValue);
		}
		OutJsonObject->SetArrayField(InName, TypesJsonValues);
	};

	auto SerializeFeatureConfigsToJsonObject = [](const FDLCFeatureConfig& InFeatureInfo, TSharedPtr<FJsonObject>& OutJsonObject)
	{
		if (!OutJsonObject.IsValid())
		{
			OutJsonObject = MakeShareable(new FJsonObject);
		}
		OutJsonObject->SetStringField(TEXT("SceneName"), InFeatureInfo.SceneName);
		OutJsonObject->SetStringField(TEXT("SceneType"), InFeatureInfo.SceneType);
		OutJsonObject->SetStringField(TEXT("ScenePriority"), InFeatureInfo.ScenePriority);
	};

	auto SerializeAssetMandatoryIncludeFilterToJsonObject = [](const FDirectoryPath& InDirectoryPath, TSharedPtr<FJsonObject>& OutJsonObject)
	{
		if (!OutJsonObject.IsValid())
		{
			OutJsonObject = MakeShareable(new FJsonObject);
		}
		OutJsonObject->SetStringField(TEXT("Path"), InDirectoryPath.Path);
	};

	auto SerializePakSizeToJsonObject = [](const FINTMap& InFIntMap, TSharedPtr<FJsonObject>& OutJsonObject)
	{
		if (!OutJsonObject.IsValid())
		{
			OutJsonObject = MakeShareable(new FJsonObject);
		}
		
	};

	auto SerializeAddExternAssetsToPlatformToJsonObject = [](const FPlatformExternAssetsEx& InExternAssetsInfo, TSharedPtr<FJsonObject>& OutJsonObject)
	{
		if (!OutJsonObject.IsValid())
		{
			OutJsonObject = MakeShareable(new FJsonObject);
		}
		OutJsonObject->SetStringField(TEXT("PakName"), InExternAssetsInfo.PakName);
		OutJsonObject->SetStringField(TEXT("SceneType"), InExternAssetsInfo.SceneType);
		OutJsonObject->SetBoolField(TEXT("bIteratePak"), InExternAssetsInfo.bIteratePak);

		FString TargetPlatform = UFlibPatchParserHelper::GetEnumNameByValue(InExternAssetsInfo.TargetPlatform);
		OutJsonObject->SetStringField(TEXT("TargetPlatform"), TargetPlatform);

		TArray<TSharedPtr<FJsonValue>> AddExternFileToPakJsonObjectList;
		for (const auto& ExternFileInfo : InExternAssetsInfo.AddExternFileToPak)
		{
			TSharedPtr<FJsonObject> CurrentFileJsonObject;
			
			if (!CurrentFileJsonObject.IsValid())
			{
				CurrentFileJsonObject = MakeShareable(new FJsonObject);
			}
			CurrentFileJsonObject->SetStringField(TEXT("FilePath"), ExternFileInfo.FilePath.FilePath);
			CurrentFileJsonObject->SetStringField(TEXT("MountPath"), ExternFileInfo.MountPath);

			AddExternFileToPakJsonObjectList.Add(MakeShareable(new FJsonValueObject(CurrentFileJsonObject)));
		}
		OutJsonObject->SetArrayField(TEXT("AddExternFileToPak"), AddExternFileToPakJsonObjectList);

		TArray<TSharedPtr<FJsonValue>> AddExternDirectoryToPakJsonObjectList;
		for (const auto& ExternDirectoryInfo : InExternAssetsInfo.AddExternDirectoryToPak)
		{
			TSharedPtr<FJsonObject> CurrentFileJsonObject;

			if (!CurrentFileJsonObject.IsValid())
			{
				CurrentFileJsonObject = MakeShareable(new FJsonObject);
			}
			CurrentFileJsonObject->SetStringField(TEXT("DirectoryPath"), ExternDirectoryInfo.DirectoryPath.Path);
			CurrentFileJsonObject->SetStringField(TEXT("MountPoint"), ExternDirectoryInfo.MountPoint);

			AddExternDirectoryToPakJsonObjectList.Add(MakeShareable(new FJsonValueObject(CurrentFileJsonObject)));
		}
		OutJsonObject->SetArrayField(TEXT("AddExternDirectoryToPak"), AddExternDirectoryToPakJsonObjectList);
	};


	if (!OutJsonObject.IsValid())
	{
		OutJsonObject = MakeShareable(new FJsonObject);
	}
	OutJsonObject->SetStringField(TEXT("LogsFilePath"), InCreatePaksSetting->GetLogsFilePath());

	// serialize platform list
	{
		TArray<FString> AllPlatforms;
		for (const auto& Platform : InCreatePaksSetting->GetPakTargetPlatforms())
		{
			AllPlatforms.AddUnique(UFlibPatchParserHelper::GetEnumNameByValue(Platform));
		}
		SerializeArrayLambda(AllPlatforms, TEXT("PakTargetPlatforms"));
	}


	// serialize all add extern file to pak
	{
		TArray<TSharedPtr<FJsonValue>> FeatureConfigsJsonObjectList;
		for (const auto& Feature : InCreatePaksSetting->GetFeatureConfigs())
		{
			TSharedPtr<FJsonObject> CurrentFileJsonObject;
			SerializeFeatureConfigsToJsonObject(Feature, CurrentFileJsonObject);
			FeatureConfigsJsonObjectList.Add(MakeShareable(new FJsonValueObject(CurrentFileJsonObject)));
		}
		OutJsonObject->SetArrayField(TEXT("FeatureConfigs"), FeatureConfigsJsonObjectList);
	}

	{
		TArray<TSharedPtr<FJsonValue>> AddExternAssetsToPlatformJsonObjectList;
		for (const auto& ExternAssetsPlatform : InCreatePaksSetting->GetAddExternAssetsToPlatform())
		{
			TSharedPtr<FJsonObject> CurrentFileJsonObject;
			SerializeAddExternAssetsToPlatformToJsonObject(ExternAssetsPlatform, CurrentFileJsonObject);
			AddExternAssetsToPlatformJsonObjectList.Add(MakeShareable(new FJsonValueObject(CurrentFileJsonObject)));
		}
		OutJsonObject->SetArrayField(TEXT("AddExternAssetsToPlatform"), AddExternAssetsToPlatformJsonObjectList);
	}

	{
		TArray<TSharedPtr<FJsonValue>> AssetMandatoryIncludeFiltersJsonObjectList;
		for (const auto& Filter : InCreatePaksSetting->GetAssetMandatoryIncludeFilters())
		{
			TSharedPtr<FJsonObject> CurrentFileJsonObject;
			SerializeAssetMandatoryIncludeFilterToJsonObject(Filter, CurrentFileJsonObject);
			AssetMandatoryIncludeFiltersJsonObjectList.Add(MakeShareable(new FJsonValueObject(CurrentFileJsonObject)));
		}
		OutJsonObject->SetArrayField(TEXT("AssetMandatoryIncludeFilters"), AssetMandatoryIncludeFiltersJsonObjectList);
	}

	{
		TSharedPtr<FJsonObject> PakSizeControlJsonObject = MakeShareable(new FJsonObject);
		for (const auto& PakSizeTupple : InCreatePaksSetting->GetPakSizeControl())
		{
			TSharedPtr<FJsonObject> DeserializeJsonObject;
			UFlibPatchParserHelper::TSerializeStructAsJsonObject(PakSizeTupple.Value, DeserializeJsonObject);

			PakSizeControlJsonObject->SetObjectField(PakSizeTupple.Key, DeserializeJsonObject);
		}
		OutJsonObject->SetObjectField(TEXT("PakSizeControl"), PakSizeControlJsonObject);
	}

	SerializeAssetDependencyTypes(TEXT("AssetRegistryDependencyTypes"), InCreatePaksSetting->GetAssetRegistryDependencyTypes());
	OutJsonObject->SetStringField(TEXT("SavePath"), InCreatePaksSetting->GetSavePath());
	OutJsonObject->SetStringField(TEXT("VersionId"), InCreatePaksSetting->VersionId);
	OutJsonObject->SetStringField(TEXT("BuildDataPath"), InCreatePaksSetting->GetBuildDataPath());
	
	return true;
}

class UCreatePaksSettings* UFlibHotPatcherEditorHelper::DeserializeCreatePaksConfig(class UCreatePaksSettings* InNewSetting, const FString& InContent)
{
#define DESERIAL_BOOL_BY_NAME(SettingObject,JsonObject,MemberName) SettingObject->MemberName = JsonObject->GetBoolField(TEXT(#MemberName));
#define DESERIAL_STRING_BY_NAME(SettingObject,JsonObject,MemberName) SettingObject->MemberName = JsonObject->GetStringField(TEXT(#MemberName));
	TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(InContent);
	TSharedPtr<FJsonObject> JsonObject;
	if (FJsonSerializer::Deserialize(JsonReader, JsonObject))
	{
		// 
		{
			InNewSetting->LogsFilePath.Path = JsonObject->GetStringField(TEXT("LogsFilePath"));
			// TargetPlatform
			{

				TArray<TSharedPtr<FJsonValue>> TargetPlatforms;
				TargetPlatforms = JsonObject->GetArrayField(TEXT("PakTargetPlatforms"));

				TArray<ETargetPlatform> FinalTargetPlatforms;

				for (const auto& Platform : TargetPlatforms)
				{
					ETargetPlatform CurrentEnum;
					if (UFlibPatchParserHelper::GetEnumValueByName(Platform->AsString(), CurrentEnum))
					{
						FinalTargetPlatforms.Add(CurrentEnum);
					}
				}
				InNewSetting->PakTargetPlatforms = FinalTargetPlatforms;
			}

			// extern directory
			{
				TArray<FDLCFeatureConfig> FeatureConfigsArray;
				TArray<TSharedPtr<FJsonValue>> FeatureConfigsJsonValues;

				FeatureConfigsJsonValues = JsonObject->GetArrayField(TEXT("FeatureConfigs"));

				for (const auto& FileJsonValue : FeatureConfigsJsonValues)
				{
					FDLCFeatureConfig Feature;
					TSharedPtr<FJsonObject> FileJsonObjectValue = FileJsonValue->AsObject();

					Feature.SceneName = FileJsonObjectValue->GetStringField(TEXT("SceneName"));
					Feature.ScenePriority = FileJsonObjectValue->GetStringField(TEXT("ScenePriority"));
					Feature.SceneType = FileJsonObjectValue->GetStringField(TEXT("SceneType"));

					FeatureConfigsArray.AddUnique(Feature);
				}
				InNewSetting->FeatureConfigs = FeatureConfigsArray;
			}

			// deserialize AssetRegistryDependencyTypes
			{
				TArray<EAssetRegistryDependencyTypeEx> result;
				TArray<TSharedPtr<FJsonValue>> AssetRegistryDependencyTypes = JsonObject->GetArrayField(TEXT("AssetRegistryDependencyTypes"));
				for (const auto& TypeJsonValue : AssetRegistryDependencyTypes)
				{
					EAssetRegistryDependencyTypeEx CurrentType;
					if (UFlibPatchParserHelper::GetEnumValueByName(TypeJsonValue->AsString(), CurrentType))
					{
						result.AddUnique(CurrentType);
					}
				}
				InNewSetting->AssetRegistryDependencyTypes = result;
			}

			InNewSetting->SavePath.Path = JsonObject->GetStringField(TEXT("SavePath"));
			InNewSetting->VersionId = JsonObject->GetStringField(TEXT("VersionId"));
			InNewSetting->BuildDataPath.Path = JsonObject->GetStringField(TEXT("BuildDataPath"));

			// deserialize AddExternAssetsToPlatform
			{
				TArray<FPlatformExternAssetsEx> AddExternAssetsToPlatform;
				TArray<TSharedPtr<FJsonValue>> AddExternAssetsToPlatformJsonValues;

				AddExternAssetsToPlatformJsonValues = JsonObject->GetArrayField(TEXT("AddExternAssetsToPlatform"));
				for (const auto& FileJsonValue : AddExternAssetsToPlatformJsonValues)
				{
					FPlatformExternAssetsEx PlatformExternAssetsEx;
					TSharedPtr<FJsonObject> FileJsonObjectValue = FileJsonValue->AsObject();

					PlatformExternAssetsEx.PakName = FileJsonObjectValue->GetStringField(TEXT("PakName"));
					PlatformExternAssetsEx.SceneType = FileJsonObjectValue->GetStringField(TEXT("SceneType"));
					PlatformExternAssetsEx.bIteratePak = FileJsonObjectValue->GetBoolField(TEXT("bIteratePak"));

					FString TargetPlatform = FileJsonObjectValue->GetStringField(TEXT("TargetPlatform"));
					UFlibPatchParserHelper::GetEnumValueByName(TargetPlatform, PlatformExternAssetsEx.TargetPlatform);

					TArray<FExternFileInfo> AddExternFileToPak;
					TArray<TSharedPtr<FJsonValue>> AddExternFileToPakObjectValue = FileJsonObjectValue->GetArrayField(TEXT("AddExternFileToPak"));
					for (const auto& AddExternFileToPakJsonValue : AddExternFileToPakObjectValue)
					{
						FExternFileInfo ExternFileInfo;
						TSharedPtr<FJsonObject> AddExternFileToPakFileJsonObjectValue = AddExternFileToPakJsonValue->AsObject();

						ExternFileInfo.FilePath.FilePath = AddExternFileToPakFileJsonObjectValue->GetStringField(TEXT("FilePath"));
						ExternFileInfo.MountPath = AddExternFileToPakFileJsonObjectValue->GetStringField(TEXT("MountPath"));

						AddExternFileToPak.AddUnique(ExternFileInfo);
					}
					PlatformExternAssetsEx.AddExternFileToPak = AddExternFileToPak;

					TArray<FExternDirectoryInfo> AddExternDirectoryToPak;
					TArray<TSharedPtr<FJsonValue>> AddExternDirectoryToPakObjectValue = FileJsonObjectValue->GetArrayField(TEXT("AddExternDirectoryToPak"));
					for (const auto& AddExternDirectoryToPakJsonValue : AddExternDirectoryToPakObjectValue)
					{
						FExternDirectoryInfo ExternDirectoryInfo;
						TSharedPtr<FJsonObject> AddExternDirectoryToPakFileJsonObjectValue = AddExternDirectoryToPakJsonValue->AsObject();

						ExternDirectoryInfo.DirectoryPath.Path = AddExternDirectoryToPakFileJsonObjectValue->GetStringField(TEXT("DirectoryPath"));
						ExternDirectoryInfo.MountPoint = AddExternDirectoryToPakFileJsonObjectValue->GetStringField(TEXT("MountPoint"));

						AddExternDirectoryToPak.AddUnique(ExternDirectoryInfo);
					}
					PlatformExternAssetsEx.AddExternDirectoryToPak = AddExternDirectoryToPak;

					AddExternAssetsToPlatform.AddUnique(PlatformExternAssetsEx);
				}
				InNewSetting->AddExternAssetsToPlatform = AddExternAssetsToPlatform;
			}

			// deserialize AssetMandatoryIncludeFilters
			{
				TArray<FDirectoryPath> AssetMandatoryIncludeFilters;
				TArray<TSharedPtr<FJsonValue>> AssetMandatoryIncludeFiltersJsonValues;
				AssetMandatoryIncludeFiltersJsonValues = JsonObject->GetArrayField(TEXT("AssetMandatoryIncludeFilters"));
				for (const auto& FileJsonValue : AssetMandatoryIncludeFiltersJsonValues)
				{
					FDirectoryPath DirectoryPath;
					TSharedPtr<FJsonObject> FileJsonObjectValue = FileJsonValue->AsObject();
					DirectoryPath.Path = FileJsonObjectValue->GetStringField(TEXT("Path"));
					AssetMandatoryIncludeFilters.Add(DirectoryPath);
				}
				InNewSetting->AssetMandatoryIncludeFilters = AssetMandatoryIncludeFilters;
			}

			// deserialize PakSizeControl
			{
				TMap<FString, FINTMap> PakSizeControl;
				TSharedPtr<FJsonObject> PakSizeControlJsonObject;
				PakSizeControlJsonObject = JsonObject->GetObjectField(TEXT("PakSizeControl"));
				for (const auto& PakSizeControlJsonObjectItem : PakSizeControlJsonObject->Values)
				{
					FINTMap FintMapStruct;
					UFlibPatchParserHelper::TDeserializeJsonObjectAsStruct(PakSizeControlJsonObjectItem.Value->AsObject(), FintMapStruct);
					PakSizeControl.Add(PakSizeControlJsonObjectItem.Key, FintMapStruct);
				}
				InNewSetting->PakSizeControl = PakSizeControl;
			}
		}
	}

#undef DESERIAL_BOOL_BY_NAME
#undef DESERIAL_STRING_BY_NAME
	return InNewSetting;
}



