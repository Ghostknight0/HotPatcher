// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Model/FHotPatcherCreatePatchModel.h"
#include "CreatePaksSettings.h"
#include "SHotPatcherPatchableBase.h"

///BY yina
#include "FDLCFeatureConfig.h"

#include "HotPatcherEditor.h"

// engine header
#include "Interfaces/ITargetPlatform.h"
#include "Templates/SharedPointer.h"
#include "IDetailsView.h"
#include "PropertyEditorModule.h"

/**
 * Implements the cooked platforms panel.
 */
class SHotPatcherCreatePaks
	: public SHotPatcherPatchableBase
{
public:

	SLATE_BEGIN_ARGS(SHotPatcherCreatePaks) { }
	SLATE_END_ARGS()

public:

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The Slate argument list.
	 */
	void Construct(const FArguments& InArgs, TSharedPtr<FHotPatcherCreatePatchModel> InCreateModel);

public:
	virtual void ImportConfig();
	virtual void ExportConfig()const;
	virtual void ResetConfig();
	virtual void DoGenerate();

protected:
	void CreateExportFilterListView();
	bool CanCreatePaks()const;
	FReply DoCreatePaks();

	FReply DoDiff()const;
	bool CanDiff()const;
	EVisibility VisibilityDiffButtons()const;

	void SetInformationContent(const FString& InContent)const;
	void SetInfomationContentVisibility(EVisibility InVisibility)const;
	bool CanExportPatch()const;

	FReply DoCreateDiffPaks();
	bool CanCreateDiffPaks()const;

	bool GetAllAsset2PakInfo(TArray<FString>& InPackagePathList,FString& VersionPath,FString& AllAsset2PakIDFilePath);

	virtual FExportPatchSettings* GetSettingObject()
	{
		return (FExportPatchSettings*)GPatchSettings;
	}

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
	void FillPakID2AllPakFileCommondsDic_Add();

	bool ExecuteUnrealPakProcess();
	bool ExecuteUnrealPakProcess_ByDiff(TArray<FString>& PakNameList,bool is_add = false);

	bool GetPakCMDs();

	bool SpiltPakByResourceType();

	void FillTypeCountSize();

	bool GetResourceTypeMustInApk(FString& ResourceType);

	bool isPakFull(FString& AssetPath, FString& PakName, FString& resrouceType);
	bool AssetResoruceFillinPak(FString& FirstAsset, FString& PakName, FString& SceneName);
	bool AssetResoruceFillinScene(FString& AssetResoruce, FString& PakName, FString& SceneName);

	void FillTempFilesinPak(bool isDiff = false);

	bool FillSceneDic();

	void FillTypeResourceSilptDLCandAPKLevel();

	void FillDirectAndRecursionDependence();

	bool WritePakFilesInfoMapToJsons(TMap<FString, TArray<FPakFileInfo>>& PakFilesInfoMap);

	bool WriteFile2PakAssetsMapToJson(FString FileSaveName);

	void CleanData();

	void SaveVersionFile(FString version, FString file, TMap<FString, TArray<FString>> PakFiles);

	void ChangeVersionFile(FString file, TMap<FString, TArray<FString>> PakFiles);

	bool CalculateCRC(uint32& crcValue, FString path);

	bool CheckCommandNeedFiles(FString& OutCheckResult, FString& OutCryptoPath);
	bool CheckCommandNeedFiles(FString& OutCheckResult, FString& OutCryptoPath, FString& OutVersionPath, FString& OutAllAsset2PakIDFilePath,FString& OutTypeResourceFilePath,FString& OutTypePakNumFilePath, FString& OutTempFileNoSpecifyPath);

	void CopyCryptoFile(FString& InCryptoPath);

	void LoadVersionJsonFile(FString& InVersionPath);

	void LoadAllAsset2PakIDJsonFile(FString& InFilePath);

	bool DealAddAssetPak(FPatchVersionDiff& VersionDiffInfo, const ETargetPlatform& Platform, FChunkInfo& Chunk, TArray<FPakCommand>& AdditionalFileToPak, FString& AllAsset2PakIDFilePath, FString& TypeResourceFilePath, FString& TypePakNumFilePath,FString& TempFileNoSpecifyPath);

	void WriteTypeResourceToJson(FString FileSaveName,bool is_add = false);

	void WriteTypePakNumToJson(FString FileSaveName, bool is_add = false);

	void LoadTypeResourceFile(FString& InFilePath);

	void LoadTypePakNumFile(FString& InFilePath);

	void LoadTempFileNoSpecify2PakFile(FString& InFilePath);

	void WriteTempFile2PakToJson(FString FileSaveName);

	void WriteTempFileNoSpecify2PakToJson(FString FileSaveName, bool is_add = false);

	bool TempFile2PakIsContains(const FString& IniFile, TArray<FString>& RelativePathList,FString& OutRelativePath);

	void GetCookedIniRelativePath(const FString& IniFile, FString& OutCookedIniRelativePath);

	bool WriteAllAsset2PakIDByJsonObj(FString FileSaveName);

	//void WriteNewReleaseJsonFile(UPatcherProxy* PatcherProxy);
private:

	// TSharedPtr<FHotPatcherCreatePatchModel> mCreatePatchModel;

	/** Settings view ui element ptr */
	TSharedPtr<IDetailsView> SettingsView;
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
	FString m_strPlatform = "";//"Android_ASTC";

	TSharedPtr<SHotPatcherInformations> DiffWidget;
	FString m_pakListsPath = "";//"C:\UnrealProjects\testDLC7\BuildData\WindowsNoEditor\testDLC7\PakLists";

	FString m_CryptoPath;

	//TMap<FString, TArray<FString>> m_PakFiles;

	TArray<TSharedPtr<FJsonValue>> m_Paks_Item_List;

	FString m_VersionOld;
	TSharedPtr<FJsonObject> m_AllAsset2PakID_JsonObj;

	TMap<FString, FString> m_TempFile2Pak;//非asset临时文件对应的包体分配管理
	TMap<FString, FString> m_TempFileRelativePath;//ini文件使用

	TMap<FString, FString> m_TempFileNoSpecify2Pak;//非asset文件中未指定包体的文件列表
	TSharedPtr<FJsonObject> m_TempFileNoSpecify2Pak_JsonObj;
};