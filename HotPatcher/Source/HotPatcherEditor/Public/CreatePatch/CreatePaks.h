#pragma once

#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Model/FHotPatcherCreatePatchModel.h"
#include "CreatePaksSettings.h"
#include "SHotPatcherPatchableBase.h"
#include "FExternFileInfo.h"

///BY yina
#include "FDLCFeatureConfig.h"

// engine header
#include "Interfaces/ITargetPlatform.h"
#include "Templates/SharedPointer.h"
#include "IDetailsView.h"
#include "PropertyEditorModule.h"
#include "Widgets/Text/SMultiLineEditableText.h"


#include "CreatePaks.generated.h"

UCLASS()
class UCreatePaks :public UObject
{
	GENERATED_BODY()
public:

	bool CanCreatePaks();

	bool DoCreatePaks();

	static TArray<FDLCFeatureConfig> DeSerializeFDLCFeatureConfig(const FString& InJsonContent);

	bool FindAllRefFiles(FString AssetPath, TMap<FString, FString>& RefDic);

	bool WriteRecursiveDictToJson(FString FileSaveName);

	bool WriteFile2PakDictToJson(FString FileSaveName);

	void CreateDir(FString filePath);

	bool WriteScene2PaksDictToJson(FString FileSaveName);

	void ScanDirectory(TArray<FString>& Files, const FString& FilePath, const FString& Extension);
	void ScanDirectoryRecursive(TArray<FString>& Files, const FString& FilePath, const FString& Extension);

	bool GetAllPakFiles();

	bool GetPakOMandatoryFilesByResourceType();

	bool FillPakID2AllPakFileCommondsDic();

	bool ExecuteUnrealPakProcess();

	bool GetPakCMDs();

	bool SpiltPakByResourceType();

	void FillTypeCountSize();

	bool GetResourceTypeMustInApk(FString& ResourceType);

	bool isPakFull(FString& AssetPath, FString& PakName, FString& resrouceType);
	bool AssetResoruceFillinPak(FString& FirstAsset, FString& PakName, FString& SceneName);
	bool AssetResoruceFillinScene(FString& AssetResoruce, FString& PakName, FString& SceneName);

	bool FillSceneDic();

	void FillTypeResourceSilptDLCandAPKLevel();

	void FillDirectAndRecursionDependence();


	bool WritePakFilesInfoMapToJsons(TMap<FString, TArray<FPakFileInfo>> PakFilesInfoMap);

	void CleanData();

private:
	UCreatePaksSettings* m_CreatePaksSettings;


	TMap<FString, FString> m_apkMapDic;
	TMap<FString, FString> m_dlcMapDic;


	TArray<FAssetDetail> m_AllAssets;

	TArray<FHotPatcherAssetDependency> m_AssetsDependency;
	TMap<FString, TArray<FString>> m_File2DirectDependence;
	TMap<FString, TArray<FString>> m_File2RecursionDependence;
	TMap<FString, FString> m_tempRecursiveDict;


	TArray<FAssetDetail> m_AllPak0Assets;

	TMap<FString, TArray<FString>> m_TypeResource;
	TMap<FString, FAssetDetail> m_File2FAssetDetail;
	TMap<FString, TArray<INT>> m_TypePakNum;
	TMap<FString, TMap<INT, INT>> m_TypeCountSize;//临时
	TMap<FString, TMap<FString, FString>> m_PakID2AllPakFileCommondsDic;//Command


	TMap<FString, FString> m_AllAsset2PakID;
	TMap<FString, TArray<FString>> m_PakID2FileList;
	TMap<FString, TArray<FString>> m_Scene2FileList;
	TMap<FString, TArray<FString>> m_Scene2PakIDList;

	//不打Engine 估计会有问题，先打Slate/Plugin/Internationalization国际化
	TMap<FString, FString> m_AllPakFileCommandsFromPakListLogs;

	TMap<FString, TArray<FPakFileInfo>> m_PakInfo;

	FString m_logsFilePath = "";//"H:/UE425/UnrealEngine-release/Engine/Programs/AutomationTool/Saved/Logs/";
	FString m_strPlatform = "Android_ASTC";

};