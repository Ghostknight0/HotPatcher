// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

// #include "HotPatcherPrivatePCH.h"
#include "CreatePatch/SHotPatcherCreatePaks.h"
#include "FlibHotPatcherEditorHelper.h"
#include "FLibAssetManageHelperEx.h"
#include "AssetManager/FAssetDependenciesInfo.h"
#include "FHotPatcherVersion.h"
#include "CreatePatch/PatcherProxy.h"

// engine header
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Layout/SSeparator.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopedSlowTask.h"

#include "FPakFileInfo.h"
#include "ThreadUtils/FThreadUtils.hpp"
#include "PakFileUtilities.h"
#include "HotPatcherLog.h"

#define LOCTEXT_NAMESPACE "SHotPatcherCreatePaks"

void SHotPatcherCreatePaks::Construct(const FArguments& InArgs, TSharedPtr<FHotPatcherCreatePatchModel> InCreatePatchModel)
{
	CreateExportFilterListView();

	mCreatePatchModel = InCreatePatchModel;

	ChildSlot
		[

			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		[
			SettingsView->AsShared()
		]
		]

	+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Right)
		.Padding(4, 4, 10, 4)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
		.HAlign(HAlign_Right)
		.AutoWidth()
		.Padding(4, 4, 10, 4)
		[
			SNew(SButton)
			.Text(LOCTEXT("Diff", "Diff"))
		.IsEnabled(this, &SHotPatcherCreatePaks::CanDiff)
		.OnClicked(this, &SHotPatcherCreatePaks::DoDiff)
		.Visibility(this, &SHotPatcherCreatePaks::VisibilityDiffButtons)
		]
	+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		.Padding(4, 4, 10, 4)
		[
			SNew(SButton)
			.Text(LOCTEXT("GenerateDiffPatch", "CreateDiffPatch"))
		.IsEnabled(this, &SHotPatcherCreatePaks::CanCreateDiffPaks)
		.OnClicked(this, &SHotPatcherCreatePaks::DoCreateDiffPaks)
		]
	+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		.Padding(4, 4, 10, 4)
		[
			SNew(SButton)
			.Text(LOCTEXT("GenerateCreatePaks", "Create Paks"))
		.IsEnabled(this, &SHotPatcherCreatePaks::CanCreatePaks)
		.OnClicked(this, &SHotPatcherCreatePaks::DoCreatePaks)
		]
		]
	+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.Padding(4, 4, 10, 4)
		[
			SAssignNew(DiffWidget, SHotPatcherInformations)
			.Visibility(EVisibility::Collapsed)
		]

		];

	m_CreatePaksSettings = UCreatePaksSettings::Get();
	SettingsView->SetObject(m_CreatePaksSettings);
}

void SHotPatcherCreatePaks::ImportConfig()
{
	UE_LOG(LogTemp, Log, TEXT("Release Import Config"));
	TArray<FString> Files = this->OpenFileDialog();
	if (!Files.Num()) return;

	FString LoadFile = Files[0];

	FString JsonContent;
	if (UFLibAssetManageHelperEx::LoadFileToString(LoadFile, JsonContent))
	{
		UFlibHotPatcherEditorHelper::DeserializeCreatePaksConfig(m_CreatePaksSettings, JsonContent);
		SettingsView->ForceRefresh();
	}
}
void SHotPatcherCreatePaks::ExportConfig()const
{
	UE_LOG(LogTemp, Log, TEXT("Release Export Config"));
	TArray<FString> Files = this->SaveFileDialog();

	if (!Files.Num()) return;

	FString SaveToFile = Files[0].EndsWith(TEXT(".json")) ? Files[0] : Files[0].Append(TEXT(".json"));

	if (m_CreatePaksSettings)
	{
		FString SerializedJsonStr;
		m_CreatePaksSettings->SerializeCreatePaksConfigToString(SerializedJsonStr);

		if (FFileHelper::SaveStringToFile(SerializedJsonStr, *SaveToFile))
		{
			FText Msg = LOCTEXT("SavedPatchConfigMas", "Successd to Export the Patch Config.");
			UFlibHotPatcherEditorHelper::CreateSaveFileNotify(Msg, SaveToFile);
		}
	}
}

void SHotPatcherCreatePaks::ResetConfig()
{
	UE_LOG(LogTemp, Log, TEXT("Release Clear Config"));

	UCreatePaksSettings* DefaultSetting = NewObject<UCreatePaksSettings>();

	FString DefaultSettingJson;
	DefaultSetting->SerializeCreatePaksConfigToString(DefaultSettingJson);
	UFlibHotPatcherEditorHelper::DeserializeCreatePaksConfig(m_CreatePaksSettings, DefaultSettingJson);
	SettingsView->ForceRefresh();
}
void SHotPatcherCreatePaks::DoGenerate()
{
	UE_LOG(LogTemp, Log, TEXT("Release DoGenerate"));
}

/////////////////////////////////////////////////////////////////////////
void SHotPatcherCreatePaks::CreateExportFilterListView()
{
	// Create a property view
	FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bUpdatesFromSelection = true;
	DetailsViewArgs.bLockable = true;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::ComponentsAndActorsUseNameArea;
	DetailsViewArgs.bCustomNameAreaLocation = false;
	DetailsViewArgs.bCustomFilterAreaLocation = true;
	DetailsViewArgs.DefaultsOnlyVisibility = EEditDefaultsOnlyNodeVisibility::Hide;

	SettingsView = EditModule.CreateDetailView(DetailsViewArgs);
}

/// <summary>
/// 解析FDLCFeatureConfig
/// </summary>
/// <returns></returns>

TArray<FDLCFeatureConfig> SHotPatcherCreatePaks::DeSerializeFDLCFeatureConfig(const FString& InJsonContent)
{
	TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(InJsonContent);
	TSharedPtr<FJsonObject> JsonObject;
	FJsonSerializer::Deserialize(JsonReader, JsonObject);

	FDLCFeatureConfig result;
	TArray<FDLCFeatureConfig> resultArray;
	TArray<TSharedPtr<FJsonValue>> JsonValueList = JsonObject->GetArrayField("SceneList");

	for (const auto& Item : JsonValueList)
	{
		result.SceneName = Item->AsObject()->GetStringField("SceneName");
		result.SceneType = Item->AsObject()->GetStringField("SceneType");
		result.ScenePriority = Item->AsObject()->GetStringField("ScenePriority");
		UE_LOG(LogTemp, Log, TEXT("=======================KDLCPatcher is %s %s %s."), *result.SceneName, *result.SceneType, *result.ScenePriority);
		resultArray.Add(result);
	}

	return resultArray;
}

bool SHotPatcherCreatePaks::FindAllRefFiles(FString AssetPath, TMap<FString, FString>& RefDic)
{
	for (auto item : m_File2DirectDependence[AssetPath])
	{
		if (!RefDic.Contains(item))
		{
			RefDic.Add(item, item);
			FindAllRefFiles(item, RefDic);
		}
	}

	return true;
}

bool SHotPatcherCreatePaks::WriteScene2PaksDictToJson(FString FileSaveName,bool is_add)
{
	TSharedPtr<FJsonObject> RootJsonObject = MakeShareable(new FJsonObject);

	for (const auto& Scene2PakIDList : m_Scene2PakIDList)
	{
		TArray<FString> InArray;
		for (const auto& FilePathRef : Scene2PakIDList.Value)
		{
			InArray.AddUnique(FilePathRef);
		}

		TArray<TSharedPtr<FJsonValue>> ArrayJsonValueList;
		for (const auto& ArrayItem : InArray)
		{
			ArrayJsonValueList.Add(MakeShareable(new FJsonValueString(ArrayItem)));
		}

		RootJsonObject->SetArrayField(Scene2PakIDList.Key, ArrayJsonValueList);
	}

	FString OutString;
	auto JsonWriter = TJsonWriterFactory<TCHAR>::Create(&OutString);
	FJsonSerializer::Serialize(RootJsonObject.ToSharedRef(), JsonWriter);
	FString Scene2PakIDListPath = FString::Printf(TEXT("%sBuildData/%s/%s/%s"), *FPaths::ProjectDir(), *m_strPlatform, FApp::GetProjectName(), *FileSaveName);
	if (is_add)
	{
		Scene2PakIDListPath = FString::Printf(TEXT("%s/%s/%s/%s"), *m_CreatePaksSettings->GetBuildDataPath(), *m_strPlatform, FApp::GetProjectName(), *FileSaveName);
	}
	if (UFLibAssetManageHelperEx::SaveStringToFile(Scene2PakIDListPath, OutString))
	{
		auto Msg = LOCTEXT("Scene2PakIDList", "Succeed to export Scene2Pak info.");
		UFlibHotPatcherEditorHelper::CreateSaveFileNotify(Msg, Scene2PakIDListPath);
	}
	return true;
}

bool SHotPatcherCreatePaks::WriteFile2PakDictToJson(FString FileSaveName)
{
	TSharedPtr<FJsonObject> RootJsonObject = MakeShareable(new FJsonObject);

	for (const auto& assets2Pak : m_AllAsset2PakID)
	{
		RootJsonObject->SetStringField(assets2Pak.Key, assets2Pak.Value);
	}

	FString OutString;
	auto JsonWriter = TJsonWriterFactory<TCHAR>::Create(&OutString);
	FJsonSerializer::Serialize(RootJsonObject.ToSharedRef(), JsonWriter);
	FString File2PakToJson = FString::Printf(TEXT("%sBuildData/%s/%s/%s"), *FPaths::ProjectDir(), *m_strPlatform, FApp::GetProjectName(), *FileSaveName);
	if (UFLibAssetManageHelperEx::SaveStringToFile(File2PakToJson, OutString))
	{
		auto Msg = LOCTEXT("File2PakToJson", "Succeed to export Asset Related info.");
		UFlibHotPatcherEditorHelper::CreateSaveFileNotify(Msg, File2PakToJson);
	}

	return true;
}

bool SHotPatcherCreatePaks::WriteAllAsset2PakIDByJsonObj(FString FileSaveName)
{
	FString OutString;
	auto JsonWriter = TJsonWriterFactory<TCHAR>::Create(&OutString);
	FJsonSerializer::Serialize(m_AllAsset2PakID_JsonObj.ToSharedRef(), JsonWriter);
	FString File2PakToJson = FString::Printf(TEXT("%s/%s/%s/%s"), *m_CreatePaksSettings->GetBuildDataPath(), *m_strPlatform, FApp::GetProjectName(), *FileSaveName);
	if (UFLibAssetManageHelperEx::SaveStringToFile(File2PakToJson, OutString))
	{
		auto Msg = LOCTEXT("File2PakToJson", "Succeed to export Asset Related info.");
		UFlibHotPatcherEditorHelper::CreateSaveFileNotify(Msg, File2PakToJson);
	}

	return true;
}

//void SHotPatcherCreatePaks::WriteNewReleaseJsonFile()
//{
//	FString SerializeReleaseVersionInfo;
//	FHotPatcherVersion NewReleaseVersion = MakeNewRelease(BaseVersion, CurrentVersion, GetSettingObject());
//	UFlibPatchParserHelper::TSerializeStructAsJsonString(NewReleaseVersion, SerializeReleaseVersionInfo);
//
//	FString SaveCurrentVersionToFile = FPaths::Combine(
//		CurrentVersionSavePath,
//		FString::Printf(TEXT("%s_Release.json"), *CurrentVersion.VersionId)
//	);
//	UFLibAssetManageHelperEx::SaveStringToFile(SaveCurrentVersionToFile, SerializeReleaseVersionInfo);
//}

bool SHotPatcherCreatePaks::WriteRecursiveDictToJson(FString FileSaveName,bool is_add)
{
	TSharedPtr<FJsonObject> RootJsonObject = MakeShareable(new FJsonObject);

	for (const auto& File2Dependence : m_File2RecursionDependence)
	{
		TArray<FString> InArray;
		for (const auto& FilePathRef : File2Dependence.Value)
		{
			InArray.AddUnique(FilePathRef);
		}

		TArray<TSharedPtr<FJsonValue>> ArrayJsonValueList;
		for (const auto& ArrayItem : InArray)
		{
			ArrayJsonValueList.Add(MakeShareable(new FJsonValueString(ArrayItem)));
		}

		RootJsonObject->SetArrayField(File2Dependence.Key, ArrayJsonValueList);
	}

	FString OutString;
	auto JsonWriter = TJsonWriterFactory<TCHAR>::Create(&OutString);
	FJsonSerializer::Serialize(RootJsonObject.ToSharedRef(), JsonWriter);
	FString RecursiveDictToJson = FString::Printf(TEXT("%sBuildData/%s/%s/%s"), *FPaths::ProjectDir(), *m_strPlatform, FApp::GetProjectName(), *FileSaveName);
	if (is_add)
	{
		RecursiveDictToJson = FString::Printf(TEXT("%s/%s/%s/%s"), *m_CreatePaksSettings->GetBuildDataPath(), *m_strPlatform, FApp::GetProjectName(), *FileSaveName);
	}
	if (UFLibAssetManageHelperEx::SaveStringToFile(RecursiveDictToJson, OutString))
	{
		auto Msg = LOCTEXT("RecursiveDictToJson", "Succeed to export Asset Related info.");
		UFlibHotPatcherEditorHelper::CreateSaveFileNotify(Msg, RecursiveDictToJson);
	}

	return true;
}

void SHotPatcherCreatePaks::ScanDirectoryRecursive(TArray<FString>& Files, const FString& FilePath, const FString& Extension)
{
	IFileManager::Get().FindFilesRecursive(Files, *FilePath, *Extension, true, false);
}

// 遍历文件夹下指定类型文件
// Files 保存遍例到的所有文件
// FilePath 文件夹路径  如 "D:\\MyCodes\\LearnUE4Cpp\\Source\\LearnUE4Cpp\\"
// Extension 扩展名(文件类型) 如 "*.cpp"
void SHotPatcherCreatePaks::ScanDirectory(TArray<FString>& Files, const FString& FilePath, const FString& Extension)
{
	FString SearchedFiles = FilePath + Extension;
	TArray<FString> FindedFiles;

	IFileManager::Get().FindFiles(FindedFiles, *SearchedFiles, true, false);

	FString SearchFile = "";

	for (int i = 0; i < FindedFiles.Num(); i++)
	{
		SearchFile = FilePath + FindedFiles[i];
		Files.Add(SearchFile);

		GEngine->AddOnScreenDebugMessage(-1, 100, FColor::Red, SearchFile);
	}
}

bool SHotPatcherCreatePaks::GetPakCMDs()
{
	bool bResult = false;
	return bResult;
}
/// <summary>
/// 该类型资源不进dlc，比如
/// </summary>
/// <param name="ResourceType"></param>
/// <returns></returns>
bool SHotPatcherCreatePaks::GetResourceTypeMustInApk(FString& ResourceType)
{
	if (ResourceType.Equals("World", ESearchCase::IgnoreCase)
		|| ResourceType.Equals("Texture2D", ESearchCase::IgnoreCase)
		|| ResourceType.Equals("TextureCube", ESearchCase::IgnoreCase)
		|| ResourceType.Equals("MapBuildDataRegistry", ESearchCase::IgnoreCase)
		|| ResourceType.Equals("StaticMesh", ESearchCase::IgnoreCase)
		|| ResourceType.Equals("Material", ESearchCase::IgnoreCase)
		|| ResourceType.Equals("MaterialInstanceConstant", ESearchCase::IgnoreCase)
		|| ResourceType.Equals("SoundWave", ESearchCase::IgnoreCase)
		|| ResourceType.Equals("SoundCue", ESearchCase::IgnoreCase)
		|| ResourceType.Equals("SoundAttenuation", ESearchCase::IgnoreCase)
		|| ResourceType.Equals("SoundMix", ESearchCase::IgnoreCase)
		|| ResourceType.Equals("Skeleton", ESearchCase::IgnoreCase)
		|| ResourceType.Equals("SkeletalMesh", ESearchCase::IgnoreCase)
		|| ResourceType.Equals("Rig", ESearchCase::IgnoreCase)
		|| ResourceType.Equals("ReverbEffect", ESearchCase::IgnoreCase)
		|| ResourceType.Equals("PhysicsAsset", ESearchCase::IgnoreCase)
		|| ResourceType.Equals("AnimSequence", ESearchCase::IgnoreCase)
		|| ResourceType.Equals("AnimMontage", ESearchCase::IgnoreCase)
		|| ResourceType.Equals("ParticleSystem", ESearchCase::IgnoreCase)
		|| ResourceType.Equals("ParticleSystem", ESearchCase::IgnoreCase)
		)
	{
		return false;
	}
	else
	{
		return true;
	}
}

bool SHotPatcherCreatePaks::GetPakOMandatoryFilesByResourceType()
{
	bool bResult = false;

	//TArray<FString> InFilterPackagePaths = TArray<FString>{ "/Game/UI", "/Game/Effects", "/Game/Characters" };
	TArray<FString> InFilterPackagePaths = UFlibPatchParserHelper::GetDirectoryPaths(m_CreatePaksSettings->AssetMandatoryIncludeFilters);
	TArray<EAssetRegistryDependencyTypeEx> AssetRegistryDependencyTypes = TArray<EAssetRegistryDependencyTypeEx>{ EAssetRegistryDependencyTypeEx::All };
	UFLibAssetManageHelperEx::GetAssetsList(InFilterPackagePaths, AssetRegistryDependencyTypes, m_AllPak0Assets);

	FString strReourceName;
	FString strReourceNameDependence;
	FString SceneName = "Common";
	INT lastpakNum = 0;
	FString PakName = "";

	for (const auto& Item : m_AllPak0Assets)
	{
		strReourceName = Item.mPackagePath;
		SceneName = "Common";
		lastpakNum = m_TypePakNum[m_File2FAssetDetail[strReourceName].mAssetType].Last();
		PakName = FString::Printf(TEXT("%s_%s_%d_APK"), *m_strPlatform, *m_File2FAssetDetail[strReourceName].mAssetType, lastpakNum);

		if (isPakFull(strReourceName, PakName, m_File2FAssetDetail[strReourceName].mAssetType))
		{
			lastpakNum = lastpakNum + 1;
			PakName = FString::Printf(TEXT("%s_%s_%d_APK"), *m_strPlatform, *m_File2FAssetDetail[strReourceName].mAssetType, lastpakNum);
			m_TypePakNum[m_File2FAssetDetail[strReourceName].mAssetType].Add(lastpakNum);
		}
		AssetResoruceFillinPak(strReourceName, PakName, SceneName);

		////Asset Dependence
		if (m_File2RecursionDependence.Contains(strReourceName))
		{
			for (const auto& ItemDependence : m_File2RecursionDependence[strReourceName])
			{
				strReourceNameDependence = ItemDependence;
				if (!m_File2FAssetDetail.Contains(strReourceNameDependence))
				{
					UE_LOG(LogTemp, Log, TEXT("!!!!!!!!!!!!Error Error is strReourceNameDependence  %s not in m_File2FAssetDetail."), *strReourceNameDependence);
					continue;
				}
				lastpakNum = m_TypePakNum[m_File2FAssetDetail[strReourceNameDependence].mAssetType].Last();
				PakName = FString::Printf(TEXT("%s_%s_%d_APK"), *m_strPlatform, *m_File2FAssetDetail[strReourceNameDependence].mAssetType, lastpakNum);

				if (isPakFull(strReourceNameDependence, PakName, m_File2FAssetDetail[strReourceNameDependence].mAssetType))
				{
					lastpakNum = lastpakNum + 1;
					PakName = FString::Printf(TEXT("%s_%s_%d_APK"), *m_strPlatform, *m_File2FAssetDetail[strReourceNameDependence].mAssetType, lastpakNum);
					m_TypePakNum[m_File2FAssetDetail[strReourceNameDependence].mAssetType].Add(lastpakNum);
				}
				AssetResoruceFillinPak(strReourceNameDependence, PakName, SceneName);
			}
		}
	}
	bResult = true;

	return bResult;
}

//TMap<FString, TMap<INT, INT>> m_TypeCountSize;//临时
void SHotPatcherCreatePaks::FillTypeCountSize()
{
	TMap<INT, INT> tempdic;
	tempdic.Add(50, 1024 * 1024 * 10);
	m_TypeCountSize.Add("Default", tempdic);
	tempdic.Empty();
	tempdic.Add(30, 1024 * 1024 * 10);
	m_TypeCountSize.Add("Font", tempdic);
	tempdic.Empty();
	tempdic.Add(1, 1024 * 1024 * 10);
	m_TypeCountSize.Add("PrimaryAssetLabel", tempdic);
	tempdic.Empty();
	tempdic.Add(1, 1024 * 1024 * 10);
	m_TypeCountSize.Add("World", tempdic);
	tempdic.Empty();
	tempdic.Add(20, 1024 * 1024 * 10);
	m_TypeCountSize.Add("SkeletalMesh", tempdic);
	tempdic.Empty();
	tempdic.Add(30, 1024 * 1024 * 10);
	m_TypeCountSize.Add("StaticMesh", tempdic);
	tempdic.Empty();
	tempdic.Add(20, 1024 * 1024 * 10);
	m_TypeCountSize.Add("ParticleSystem", tempdic);
	tempdic.Empty();
	tempdic.Add(30, 1024 * 1024 * 10);
	m_TypeCountSize.Add("Texture2D", tempdic);
	tempdic.Empty();
	tempdic.Add(20, 1024 * 1024 * 10);
	m_TypeCountSize.Add("Material", tempdic);
	tempdic.Empty();
	tempdic.Add(20, 1024 * 1024 * 10);
	m_TypeCountSize.Add("MaterialFunction", tempdic);
	tempdic.Empty();
	tempdic.Add(20, 1024 * 1024 * 10);
	m_TypeCountSize.Add("PhysicalMaterial", tempdic);
	tempdic.Empty();
	tempdic.Add(20, 1024 * 1024 * 10);
	m_TypeCountSize.Add("MaterialInstanceConstant", tempdic);
	tempdic.Empty();
	tempdic.Add(20, 1024 * 1024 * 10);
	m_TypeCountSize.Add("BlendSpace", tempdic);
	tempdic.Empty();
	tempdic.Add(30, 1024 * 1024 * 10);
	m_TypeCountSize.Add("SoundCue", tempdic);
	tempdic.Empty();
	tempdic.Add(20, 1024 * 1024 * 10);
	m_TypeCountSize.Add("SoundWave", tempdic);
	tempdic.Empty();
	tempdic.Add(20, 1024 * 1024 * 10);
	m_TypeCountSize.Add("SoundAttenuation", tempdic);
	tempdic.Empty();
	tempdic.Add(20, 1024 * 1024 * 10);
	m_TypeCountSize.Add("SoundClass", tempdic);
	//tempdic.Empty();
	//tempdic.Add(20, 1024 * 10);
	//m_TypeCountSize.Add("SoundConcurrency", tempdic);
}

//	TMap<FString, TArray<FString>> m_TypeResource;
//  TMap<FString, FString> m_apkMapDic;
//  TMap<FString, FString> m_dlcMapDic;
void SHotPatcherCreatePaks::FillTypeResourceSilptDLCandAPKLevel()
{
	for (const auto& Item : m_CreatePaksSettings->GetFeatureConfigs())
	{
		if (Item.SceneType.Equals("apk", ESearchCase::IgnoreCase))
		{
			m_apkMapDic.Add(Item.SceneName, Item.SceneName);
			UE_LOG(LogTemp, Log, TEXT("=======================apkList is %s."), *Item.SceneName);
		}
		else
		{
			m_dlcMapDic.Add(Item.SceneName, Item.SceneName);
			UE_LOG(LogTemp, Log, TEXT("=======================dlcList is %s."), *Item.SceneName);
		}
	}

	for (const auto& Item : m_AllAssets)
	{
		if (Item.mPackagePath.StartsWith("/Engine/", ESearchCase::IgnoreCase))
		{
			continue;
		}

		if (!m_TypeResource.Contains(Item.mAssetType))
		{
			TArray<FString> assetsList = TArray<FString>{ Item.mPackagePath };
			m_TypeResource.Add(Item.mAssetType, assetsList);
			UE_LOG(LogTemp, Log, TEXT("=======================Item.mAssetType is %s %s."), *Item.mAssetType, *Item.mPackagePath);
		}
		else
		{
			m_TypeResource[Item.mAssetType].AddUnique(Item.mPackagePath);
		}

		if (Item.mAssetType.Equals("World", ESearchCase::IgnoreCase))
		{
			auto mapPath = FPaths::GetBaseFilename(Item.mPackagePath, true);
			//auto mapPath = Item.mPackagePath;
			UE_LOG(LogTemp, Log, TEXT("=======================umap is %s."), *mapPath);
			if (!m_apkMapDic.Contains(mapPath))
			{
				m_dlcMapDic.Add(mapPath, Item.mPackagePath);
				UE_LOG(LogTemp, Log, TEXT("=======================dlcList is %s."), *mapPath);
			}
			else
			{
				m_apkMapDic[mapPath] = Item.mPackagePath;
			}
		}
	}
}

bool SHotPatcherCreatePaks::isPakFull(FString& AssetPath, FString& PakName, FString& resrouceType)
{
	bool bResult = false;
	bool bCount = false;
	bool bSize = false;
	//bool bSize = false;

	if (AssetPath.Contains("/Engine/") || AssetPath.Contains("/Game/Config/") || AssetPath.Contains("/Game/Plugins/") ||
		AssetPath.Contains("\\Engine\\") || AssetPath.Contains("\\Game\\Config\\") || AssetPath.Contains("\\Game\\Plugins\\"))
	{
		return bResult;
	}

	if (m_PakID2FileList.Contains(PakName))
	{
		if (m_TypeCountSize.Contains(resrouceType))
		{
			if (m_PakID2FileList[PakName].Num() < m_TypeCountSize[resrouceType].begin().Key())
			{
				bCount = false;
			}
			else
			{
				bCount = true;
			}
		}
		else
		{
			if (m_PakID2FileList[PakName].Num() < m_TypeCountSize["Default"].begin().Key())
			{
				bCount = false;
			}
			else
			{
				bCount = true;
			}
		}
	}
	else
	{
		TArray<FString> TempArray;
		TempArray.Add(AssetPath);
		m_PakID2FileList.Add(PakName, TempArray);
		bCount = false;
		return bCount;
	}

	//todo add size control
	FAssetData CurrentAssetData;
	int64 cur_size;
	if (UFLibAssetManageHelperEx::GetSingleAssetsData(AssetPath, CurrentAssetData))
	{
		UFLibAssetManageHelperEx::GetAssetPackageDiskSize(CurrentAssetData.PackageName.ToString(), cur_size);
	}

	int64 totalsize = 0;
	int64 singlesize;
	for (const auto& AssetPackagePath: m_PakID2FileList[PakName])
	{
		if (UFLibAssetManageHelperEx::GetSingleAssetsData(AssetPackagePath, CurrentAssetData))
		{
			UFLibAssetManageHelperEx::GetAssetPackageDiskSize(CurrentAssetData.PackageName.ToString(), singlesize);
			totalsize += singlesize;
		}
	}

	totalsize += cur_size;

	if (m_TypeCountSize.Contains(resrouceType))
	{
		if (totalsize < m_TypeCountSize[resrouceType].begin().Value())
		{
			bSize = false;
		}
		else
		{
			bSize = true;
		}
	}
	else
	{
		if (totalsize < m_TypeCountSize["Default"].begin().Value())
		{
			bSize = false;
		}
		else
		{
			bSize = true;
		}
	}
	
	bResult = bCount || bSize;

	return bResult;
}

// TMap<FString, TArray<FString>> m_File2DirectDependence;
// TMap<FString, TArray<FString>> m_File2RecursionDependence;
void SHotPatcherCreatePaks::FillDirectAndRecursionDependence(bool is_add)
{
	/// <summary>
	/// 找到循环依赖
	/// </summary>
	/// <returns></returns>
	///

	for (const auto& AssetsDependencyItem : m_AssetsDependency)
	{
		TArray<FString> tempArray;
		if (!m_File2DirectDependence.Contains(AssetsDependencyItem.Asset.mPackagePath))
		{
			tempArray.AddUnique(AssetsDependencyItem.Asset.mPackagePath);
			m_File2DirectDependence.Emplace(AssetsDependencyItem.Asset.mPackagePath, tempArray);
		}
		for (const auto& Item : AssetsDependencyItem.AssetDependency)
		{
			TArray<FString> tempArrayRef;
			if (!m_File2DirectDependence.Contains(Item.mPackagePath))
			{
				tempArray.AddUnique(Item.mPackagePath);
				m_File2DirectDependence.Emplace(Item.mPackagePath, tempArrayRef);
			}
			m_File2DirectDependence[AssetsDependencyItem.Asset.mPackagePath].AddUnique(Item.mPackagePath);
		}
	}

	for (const auto& Item : m_File2DirectDependence)
	{
		TArray<FString> tempArray;
		if (!m_File2RecursionDependence.Contains(Item.Key))
		{
			m_File2RecursionDependence.Emplace(Item.Key, tempArray);
		}
	}

	for (const auto& Item : m_File2DirectDependence)
	{
		m_tempRecursiveDict.Empty();
		m_tempRecursiveDict.Add(Item.Key, Item.Key);
		FindAllRefFiles(Item.Key, m_tempRecursiveDict);
		TArray<FString> temp;
		for (auto i : m_tempRecursiveDict)
		{
			temp.Add(i.Key);
		}
		m_File2RecursionDependence[Item.Key].Append(temp);
	}

	m_File2RecursionDependence.KeySort([](FString A, FString B) {return A > B; });

	WriteRecursiveDictToJson("RecursiveDictToJson.json", is_add);
}

bool SHotPatcherCreatePaks::AssetResoruceFillinPak(FString& AssetResoruce, FString& PakName, FString& SceneName)
{
	bool bResult = false;
	FString PakName_EngineAndTemp = FString::Printf(TEXT("%s_EngineAndTemp_0_Apk"), *m_strPlatform);
	//FirstAsset
	if (m_AllAsset2PakID.Contains(AssetResoruce))
	{
	}
	else
	{
		if (AssetResoruce.Contains("/Engine/") || AssetResoruce.Contains("/Game/Config/") || AssetResoruce.Contains("/Game/Plugins/") ||
			AssetResoruce.Contains("\\Engine\\") || AssetResoruce.Contains("\\Game\\Config\\") || AssetResoruce.Contains("\\Game\\Plugins\\"))
		{
			PakName = PakName_EngineAndTemp;
		}

		m_AllAsset2PakID.Add(AssetResoruce, PakName);

		if (m_PakID2FileList.Contains(PakName))
		{
			m_PakID2FileList[PakName].AddUnique(AssetResoruce);
		}
		else
		{
			TArray<FString> TempArray;
			TempArray.AddUnique(AssetResoruce);
			m_PakID2FileList.Add(PakName, TempArray);
		}
	}

	if (m_Scene2FileList.Contains(SceneName))
	{
		m_Scene2FileList[SceneName].AddUnique(AssetResoruce);
	}
	else
	{
		TArray<FString> TempArray;
		TempArray.AddUnique(AssetResoruce);
		m_Scene2FileList.Add(SceneName, TempArray);
	}

	if (m_Scene2PakIDList.Contains(SceneName))
	{
		m_Scene2PakIDList[SceneName].AddUnique(PakName);
	}
	else
	{
		TArray<FString> TempPakNameArray;
		TempPakNameArray.AddUnique(PakName);
		m_Scene2PakIDList.Add(SceneName, TempPakNameArray);
	}
	UE_LOG(LogTemp, Log, TEXT("######\t%s\t%s\t%s"), *PakName, *SceneName, *AssetResoruce);

	return bResult;
}

bool SHotPatcherCreatePaks::FillSceneDic()
{
	bool bResult = false;
	FString strReourceName = "";
	FString strDependenceReourceName = "";
	FString SceneName = "";
	for (const auto& Item : m_apkMapDic)
	{
		if (Item.Value.Equals("Common", ESearchCase::IgnoreCase))
		{
			continue;
		}

		strReourceName = Item.Value;
		SceneName = Item.Value;

		if (!m_AllAsset2PakID.Contains(strReourceName))
		{
			UE_LOG(LogTemp, Log, TEXT("!!!!!!!!!!!!Error Error is %s not in m_AllAsset2PakID."), *strReourceName);
			continue;
		}

		AssetResoruceFillinScene(strReourceName, m_AllAsset2PakID[strReourceName], SceneName);

		////Asset Dependence
		if (m_File2RecursionDependence.Contains(strReourceName))
		{
			for (const auto& ItemDependence : m_File2RecursionDependence[strReourceName])
			{
				strDependenceReourceName = ItemDependence;
				if (!m_AllAsset2PakID.Contains(strDependenceReourceName))
				{
					UE_LOG(LogTemp, Log, TEXT("!!!!!!!!!!!!Error Error is strDependenceReourceName  %s not in m_AllAsset2PakID."), *strDependenceReourceName);
					continue;
				}
				AssetResoruceFillinScene(strDependenceReourceName, m_AllAsset2PakID[strDependenceReourceName], SceneName);
			}
		}
	}

	for (const auto& Item : m_dlcMapDic)
	{
		if (Item.Value.Equals("Common", ESearchCase::IgnoreCase))
		{
			continue;
		}

		strReourceName = Item.Value;
		SceneName = Item.Value;

		if (!m_AllAsset2PakID.Contains(strReourceName))
		{
			UE_LOG(LogTemp, Log, TEXT("!!!!!!!!!!!!Error Error strReourceName is %s not in m_AllAsset2PakID."), *strReourceName);
			continue;
		}

		AssetResoruceFillinScene(strReourceName, m_AllAsset2PakID[strReourceName], SceneName);

		////Asset Dependence
		if (m_File2RecursionDependence.Contains(strReourceName))
		{
			for (const auto& ItemDependence : m_File2RecursionDependence[strReourceName])
			{
				strDependenceReourceName = ItemDependence;

				if (!m_AllAsset2PakID.Contains(strDependenceReourceName))
				{
					UE_LOG(LogTemp, Log, TEXT("!!!!!!!!!!!!Error Error strDependenceReourceName is %s not in m_AllAsset2PakID."), *strDependenceReourceName);
					continue;
				}
				AssetResoruceFillinScene(strDependenceReourceName, m_AllAsset2PakID[strDependenceReourceName], SceneName);
			}
		}
	}

	bResult = true;
	return bResult;
}

bool SHotPatcherCreatePaks::FillSceneDic_add()
{
	bool bResult = false;
	FString strReourceName = "";
	FString strDependenceReourceName = "";
	FString SceneName = "";
	for (const auto& Item : m_apkMapDic)
	{
		if (Item.Value.Equals("Common", ESearchCase::IgnoreCase))
		{
			continue;
		}

		strReourceName = Item.Value;
		SceneName = Item.Value;

		//if (!m_AllAsset2PakID.Contains(strReourceName))
		FString CheckPakName;
		if (!m_AllAsset2PakID_JsonObj->TryGetStringField(strReourceName, CheckPakName))
		{
			UE_LOG(LogTemp, Log, TEXT("!!!!!!!!!!!!Error Error is %s not in m_AllAsset2PakID_JsonObj."), *strReourceName);
			continue;
		}
		FString PakName;
		PakName = m_AllAsset2PakID_JsonObj->GetStringField(strReourceName);
		AssetResoruceFillinScene(strReourceName, PakName, SceneName);

		////Asset Dependence
		if (m_File2RecursionDependence.Contains(strReourceName))
		{
			for (const auto& ItemDependence : m_File2RecursionDependence[strReourceName])
			{
				strDependenceReourceName = ItemDependence;
				//if (!m_AllAsset2PakID.Contains(strDependenceReourceName))
				if (!m_AllAsset2PakID_JsonObj->TryGetStringField(strDependenceReourceName, CheckPakName))
				{
					UE_LOG(LogTemp, Log, TEXT("!!!!!!!!!!!!Error Error is strDependenceReourceName  %s not in m_AllAsset2PakID_JsonObj."), *strDependenceReourceName);
					continue;
				}
				PakName = m_AllAsset2PakID_JsonObj->GetStringField(strDependenceReourceName);
				AssetResoruceFillinScene(strDependenceReourceName, PakName, SceneName);
			}
		}
	}

	for (const auto& Item : m_dlcMapDic)
	{
		if (Item.Value.Equals("Common", ESearchCase::IgnoreCase))
		{
			continue;
		}

		strReourceName = Item.Value;
		SceneName = Item.Value;

		//if (!m_AllAsset2PakID.Contains(strReourceName))
		FString CheckPakName;
		if (!m_AllAsset2PakID_JsonObj->TryGetStringField(strReourceName, CheckPakName))
		{
			UE_LOG(LogTemp, Log, TEXT("!!!!!!!!!!!!Error Error strReourceName is %s not in m_AllAsset2PakID_JsonObj."), *strReourceName);
			continue;
		}

		FString PakName;
		PakName = m_AllAsset2PakID_JsonObj->GetStringField(strReourceName);
		AssetResoruceFillinScene(strReourceName, PakName, SceneName);

		////Asset Dependence
		if (m_File2RecursionDependence.Contains(strReourceName))
		{
			for (const auto& ItemDependence : m_File2RecursionDependence[strReourceName])
			{
				strDependenceReourceName = ItemDependence;

				//if (!m_AllAsset2PakID.Contains(strDependenceReourceName))
				if (!m_AllAsset2PakID_JsonObj->TryGetStringField(strDependenceReourceName, CheckPakName))
				{
					UE_LOG(LogTemp, Log, TEXT("!!!!!!!!!!!!Error Error strDependenceReourceName is %s not in m_AllAsset2PakID_JsonObj."), *strDependenceReourceName);
					continue;
				}
				PakName = m_AllAsset2PakID_JsonObj->GetStringField(strDependenceReourceName);
				AssetResoruceFillinScene(strDependenceReourceName, PakName, SceneName);
			}
		}
	}

	bResult = true;
	return bResult;
}

bool SHotPatcherCreatePaks::AssetResoruceFillinScene(FString& AssetResoruce, FString& PakName, FString& SceneName)
{
	bool bResult = false;

	if (m_Scene2FileList.Contains(SceneName))
	{
		m_Scene2FileList[SceneName].AddUnique(AssetResoruce);
	}
	else
	{
		TArray<FString> TempArray;
		TempArray.AddUnique(AssetResoruce);
		m_Scene2FileList.Add(SceneName, TempArray);
	}

	if (m_Scene2PakIDList.Contains(SceneName))
	{
		m_Scene2PakIDList[SceneName].AddUnique(PakName);
	}
	else
	{
		TArray<FString> TempPakNameArray;
		TempPakNameArray.AddUnique(PakName);
		m_Scene2PakIDList.Add(SceneName, TempPakNameArray);
	}

	return bResult;
}

void SHotPatcherCreatePaks::FillTempFilesinPak(bool isDiff)
{
	FString ProjectAbsDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	FString ProjectName = UFlibPatchParserHelper::GetProjectName();

	FString CookedIniRelativePath;

	//获取AssetRegistry.bin文件
	FString AssetRegistryCookCommand;
	FString AssetRegistryPakName = FString::Printf(TEXT("%s_AssetRegistry_0_Apk"), *m_strPlatform);
	if (isDiff)
	{
		//增量更新时
		AssetRegistryPakName = FString::Printf(TEXT("%s_AssetRegistry_0_Apk_001_P"), *m_strPlatform);
	}
	UFlibPatchParserHelper::GetCookedAssetRegistryFiles(ProjectAbsDir, UFlibPatchParserHelper::GetProjectName(), m_strPlatform, AssetRegistryCookCommand);
	m_TempFile2Pak.Add(AssetRegistryCookCommand, AssetRegistryPakName);
	//GetCookedIniRelativePath(AssetRegistryCookCommand, CookedIniRelativePath);
	//m_TempFileRelativePath.Add(CookedIniRelativePath, AssetRegistryPakName);

	//获取GlobalShaderCache文件
	FString GlobalShaderCachePakName = FString::Printf(TEXT("%s_GlobalShaderCache_0_Apk"), *m_strPlatform);
	if (isDiff)
	{
		//增量更新时
		GlobalShaderCachePakName = FString::Printf(TEXT("%s_GlobalShaderCache_0_Apk_001_P"), *m_strPlatform);
	}
	TArray<FString> GlobalShaderCacheList = UFlibPatchParserHelper::GetCookedGlobalShaderCacheFiles(ProjectAbsDir, m_strPlatform);
	for (const auto& GlobalShaderCacheFile : GlobalShaderCacheList)
	{
		m_TempFile2Pak.Add(GlobalShaderCacheFile, GlobalShaderCachePakName);
		//GetCookedIniRelativePath(GlobalShaderCacheFile, CookedIniRelativePath);
		//m_TempFileRelativePath.Add(CookedIniRelativePath, GlobalShaderCachePakName);
	}

	//获取ShaderBytecode文件
	FString ShaderBytecodePakName = FString::Printf(TEXT("%s_ShaderBytecode_0_Apk"), *m_strPlatform);
	if (isDiff)
	{
		//增量更新时
		ShaderBytecodePakName = FString::Printf(TEXT("%s_ShaderBytecode_0_Apk_001_P"), *m_strPlatform);
	}
	TArray<FString> ShaderByteCodeFiles;
	UFlibPatchParserHelper::GetCookedShaderBytecodeFiles(ProjectAbsDir, ProjectName, m_strPlatform, true, true, ShaderByteCodeFiles);
	for (const auto& ShaderByteCodeFile : ShaderByteCodeFiles)
	{
		m_TempFile2Pak.Add(ShaderByteCodeFile, ShaderBytecodePakName);
		//GetCookedIniRelativePath(ShaderByteCodeFile, CookedIniRelativePath);
		//m_TempFileRelativePath.Add(CookedIniRelativePath, ShaderBytecodePakName);
	}

	//获取Ini文件
	FString IniConfingPakName = FString::Printf(TEXT("%s_IniConfing_0_Apk"), *m_strPlatform);
	if (isDiff)
	{
		//增量更新时
		IniConfingPakName = FString::Printf(TEXT("%s_IniConfing_0_Apk_001_P"), *m_strPlatform);
	}
	TArray<FString> AllNeedPakIniFiles;
	AllNeedPakIniFiles.Append(UFlibPatchParserHelper::GetEngineConfigs(m_strPlatform));
	AllNeedPakIniFiles.Append(UFlibPatchParserHelper::GetEnabledPluginConfigs(m_strPlatform));
	AllNeedPakIniFiles.Append(UFlibPatchParserHelper::GetProjectIniFiles(FPaths::ProjectDir(), m_strPlatform));
	for (const auto& IniFile : AllNeedPakIniFiles)
	{
		m_TempFile2Pak.Add(IniFile, IniConfingPakName);
		GetCookedIniRelativePath(IniFile, CookedIniRelativePath);
		m_TempFileRelativePath.Add(CookedIniRelativePath, IniConfingPakName);
	}
}

void SHotPatcherCreatePaks::FillExternFilesInPak(bool is_add)
{
	//根据平台配置的指定路径，获取文件
	if (!!m_CreatePaksSettings->AddExternAssetsToPlatform.Num())
	{
		FString ExternPakName;

		ETargetPlatform Platform;
		UFlibPatchParserHelper::GetEnumValueByName(m_strPlatform, Platform);

		for (const auto& PlatformExternAssetsEx : m_CreatePaksSettings->AddExternAssetsToPlatform)
		{
			if (PlatformExternAssetsEx.TargetPlatform == Platform || PlatformExternAssetsEx.TargetPlatform == ETargetPlatform::AllPlatforms)
			{
				//设置包名
				ExternPakName = FString::Printf(TEXT("%s_%s_Extern_0_%s"), *m_strPlatform, *PlatformExternAssetsEx.PakName, *PlatformExternAssetsEx.SceneType);

				//增量更新迭代时，包名不能重复
				if (is_add && PlatformExternAssetsEx.bIteratePak)
				{
					ExternPakName = FString::Printf(TEXT("%s_%s_Extern_%s_%s"), *m_strPlatform, *PlatformExternAssetsEx.PakName, *m_CreatePaksSettings->VersionId , *PlatformExternAssetsEx.SceneType);
				}

				//添加指定文件
				for (const auto& ExternFils : PlatformExternAssetsEx.AddExternFileToPak)
				{
					m_ExternFiles2Pak.Add(ExternFils.FilePath.FilePath, ExternPakName);
				}
				//添加指定目录路径
				for (const auto& ExternDirectory : PlatformExternAssetsEx.AddExternDirectoryToPak)
				{
					m_ExternDirectory2Pak.Add(ExternDirectory.DirectoryPath.Path, ExternPakName);
				}
			}
		}
	}
}

bool SHotPatcherCreatePaks::IsInExternDir(FString& FilePath,FString& OutDirectoryPath)
{
	bool result = false;
	for (const auto& DirectoryPath : m_ExternDirectory2Pak)
	{
		if (FilePath.Contains(DirectoryPath.Key))
		{
			result = true;
			OutDirectoryPath = DirectoryPath.Key;
			break;
		}
	}

	return result;
}


bool SHotPatcherCreatePaks::SpiltPakByResourceType()
{
	bool bResult = false;

	TArray<FAssetDetail> EngineAndGameAllAssets;
	TArray<FString> InFilterPackagePaths = TArray<FString>{ "/Game", "/Engine", "/Plugins" };
	TArray<EAssetRegistryDependencyTypeEx> AssetRegistryDependencyTypes = TArray<EAssetRegistryDependencyTypeEx>{ EAssetRegistryDependencyTypeEx::All };
	UFLibAssetManageHelperEx::GetAssetsList(InFilterPackagePaths, AssetRegistryDependencyTypes, EngineAndGameAllAssets);
	for (const auto& Item : EngineAndGameAllAssets)
	{
		//m_File2FAssetDetail m_TypePakNum 必须是全部资源，否则会找不到对应的资源类型
		m_File2FAssetDetail.Add(Item.mPackagePath, Item);
		if (!m_TypePakNum.Contains(Item.mAssetType))
		{
			INT pakNum = 0;
			TArray<INT> temp;
			temp.Add(pakNum);
			m_TypePakNum.Add(Item.mAssetType, temp);
		}
	}
	for (auto type : m_TypeResource)
	{
		if (!m_TypePakNum.Contains(type.Key))
		{
			INT pakNum = 0;
			TArray<INT> temp;
			temp.Add(pakNum);
			m_TypePakNum.Add(type.Key, temp);
		}
	}

	FillTypeCountSize();

	//先处理ui effect等
	GetPakOMandatoryFilesByResourceType();

	FString lastScene = "Common";
	FString strReourceName;
	FString strReourceNameDependence;
	FString SceneName = "Common";
	INT lastpakNum = 0;
	FString PakName = "";

	for (const auto& Item : m_apkMapDic)
	{
		if (Item.Value.Equals("Common", ESearchCase::IgnoreCase))
		{
			continue;
		}

		if (m_AllAsset2PakID.Contains(Item.Value))
		{
			continue;
		}

		strReourceName = Item.Value;
		SceneName = Item.Value;

		if (!m_File2FAssetDetail.Contains(strReourceName))
		{
			UE_LOG(LogTemp, Log, TEXT("!!!!!!!!!!!!Error Error is %s not in m_File2FAssetDetail."), *strReourceName);
			continue;
		}

		lastpakNum = m_TypePakNum[m_File2FAssetDetail[strReourceName].mAssetType].Last();
		PakName = FString::Printf(TEXT("%s_%s_%d_APK"), *m_strPlatform, *m_File2FAssetDetail[strReourceName].mAssetType, lastpakNum);

		//换场景要换bundle
		if (!Item.Key.Equals(lastScene))
		{
			lastpakNum = lastpakNum + 1;
			PakName = FString::Printf(TEXT("%s_%s_%d_APK"), *m_strPlatform, *m_File2FAssetDetail[strReourceName].mAssetType, lastpakNum);
			m_TypePakNum[m_File2FAssetDetail[strReourceName].mAssetType].Add(lastpakNum);
		}
		else
		{
			if (isPakFull(strReourceName, PakName, m_File2FAssetDetail[strReourceName].mAssetType))
			{
				lastpakNum = lastpakNum + 1;
				PakName = FString::Printf(TEXT("%s_%s_%d_APK"), *m_strPlatform, *m_File2FAssetDetail[strReourceName].mAssetType, lastpakNum);
				m_TypePakNum[m_File2FAssetDetail[strReourceName].mAssetType].Add(lastpakNum);
			}
		}
		//记录场景名
		lastScene = Item.Key;
		AssetResoruceFillinPak(strReourceName, PakName, SceneName);

		////Asset Dependence
		if (m_File2RecursionDependence.Contains(strReourceName))
		{
			for (const auto& ItemDependence : m_File2RecursionDependence[strReourceName])
			{
				if (m_AllAsset2PakID.Contains(ItemDependence))
				{
					continue;
				}
				strReourceNameDependence = ItemDependence;
				if (!m_File2FAssetDetail.Contains(strReourceNameDependence))
				{
					UE_LOG(LogTemp, Log, TEXT("!!!!!!!!!!!!Error Error is strReourceNameDependence  %s not in m_File2FAssetDetail."), *strReourceNameDependence);
					continue;
				}
				lastpakNum = m_TypePakNum[m_File2FAssetDetail[strReourceNameDependence].mAssetType].Last();
				PakName = FString::Printf(TEXT("%s_%s_%d_APK"), *m_strPlatform, *m_File2FAssetDetail[strReourceNameDependence].mAssetType, lastpakNum);

				//换场景要换bundle
				if (!Item.Key.Equals(lastScene))
				{
					lastpakNum = lastpakNum + 1;
					PakName = FString::Printf(TEXT("%s_%s_%d_APK"), *m_strPlatform, *m_File2FAssetDetail[strReourceNameDependence].mAssetType, lastpakNum);
					m_TypePakNum[m_File2FAssetDetail[strReourceNameDependence].mAssetType].Add(lastpakNum);
				}
				else
				{
					if (isPakFull(strReourceNameDependence, PakName, m_File2FAssetDetail[strReourceNameDependence].mAssetType))
					{
						lastpakNum = lastpakNum + 1;
						PakName = FString::Printf(TEXT("%s_%s_%d_APK"), *m_strPlatform, *m_File2FAssetDetail[strReourceNameDependence].mAssetType, lastpakNum);
						m_TypePakNum[m_File2FAssetDetail[strReourceNameDependence].mAssetType].Add(lastpakNum);
					}
				}
				//记录场景名
				lastScene = Item.Key;
				AssetResoruceFillinPak(strReourceNameDependence, PakName, SceneName);
			}
		}
	}

	for (const auto& Item : m_dlcMapDic)
	{
		if (Item.Value.Equals("Common", ESearchCase::IgnoreCase))
		{
			continue;
		}

		if (m_AllAsset2PakID.Contains(Item.Value))
		{
			continue;
		}
		if (!m_File2FAssetDetail.Contains(strReourceName))
		{
			UE_LOG(LogTemp, Log, TEXT("!!!!!!!!!!!!Error Error is %s not in m_File2FAssetDetail."), *strReourceName);
			continue;
		}
		strReourceName = Item.Value;
		SceneName = Item.Value;
		lastpakNum = m_TypePakNum[m_File2FAssetDetail[strReourceName].mAssetType].Last();
		PakName = FString::Printf(TEXT("%s_%s_%d_DLC"), *m_strPlatform, *m_File2FAssetDetail[strReourceName].mAssetType, lastpakNum);
		if (GetResourceTypeMustInApk(m_File2FAssetDetail[strReourceName].mAssetType))
		{
			PakName = FString::Printf(TEXT("%s_%s_%d_APK"), *m_strPlatform, *m_File2FAssetDetail[strReourceName].mAssetType, lastpakNum);
		}
		//换场景要换bundle
		if (!Item.Key.Equals(lastScene))
		{
			lastpakNum = lastpakNum + 1;
			PakName = FString::Printf(TEXT("%s_%s_%d_DLC"), *m_strPlatform, *m_File2FAssetDetail[strReourceName].mAssetType, lastpakNum);
			if (GetResourceTypeMustInApk(m_File2FAssetDetail[strReourceName].mAssetType))
			{
				PakName = FString::Printf(TEXT("%s_%s_%d_APK"), *m_strPlatform, *m_File2FAssetDetail[strReourceName].mAssetType, lastpakNum);
			}
			m_TypePakNum[m_File2FAssetDetail[strReourceName].mAssetType].Add(lastpakNum);
		}
		else
		{
			if (isPakFull(strReourceName, PakName, m_File2FAssetDetail[strReourceName].mAssetType))
			{
				lastpakNum = lastpakNum + 1;
				PakName = FString::Printf(TEXT("%s_%s_%d_DLC"), *m_strPlatform, *m_File2FAssetDetail[strReourceName].mAssetType, lastpakNum);
				if (GetResourceTypeMustInApk(m_File2FAssetDetail[strReourceName].mAssetType))
				{
					PakName = FString::Printf(TEXT("%s_%s_%d_APK"), *m_strPlatform, *m_File2FAssetDetail[strReourceName].mAssetType, lastpakNum);
				}
				m_TypePakNum[m_File2FAssetDetail[strReourceName].mAssetType].Add(lastpakNum);
			}
		}
		//记录场景名
		lastScene = Item.Key;
		AssetResoruceFillinPak(strReourceName, PakName, SceneName);

		////Asset Dependence
		if (m_File2RecursionDependence.Contains(strReourceName))
		{
			//Process Dependence Resources
			for (const auto& ItemDependence : m_File2RecursionDependence[strReourceName])
			{
				if (m_AllAsset2PakID.Contains(ItemDependence))
				{
					continue;
				}

				strReourceNameDependence = ItemDependence;
				if (!m_File2FAssetDetail.Contains(strReourceNameDependence))
				{
					UE_LOG(LogTemp, Log, TEXT("!!!!!!!!!!!!Error Error is strReourceNameDependence  %s not in m_File2FAssetDetail."), *strReourceNameDependence);
					continue;
				}
				lastpakNum = m_TypePakNum[m_File2FAssetDetail[strReourceNameDependence].mAssetType].Last();
				PakName = FString::Printf(TEXT("%s_%s_%d_DLC"), *m_strPlatform, *m_File2FAssetDetail[strReourceNameDependence].mAssetType, lastpakNum);
				if (GetResourceTypeMustInApk(m_File2FAssetDetail[strReourceNameDependence].mAssetType))
				{
					PakName = FString::Printf(TEXT("%s_%s_%d_APK"), *m_strPlatform, *m_File2FAssetDetail[strReourceNameDependence].mAssetType, lastpakNum);
				}

				//换场景要换bundle
				if (!Item.Key.Equals(lastScene))
				{
					lastpakNum = lastpakNum + 1;
					PakName = FString::Printf(TEXT("%s_%s_%d_DLC"), *m_strPlatform, *m_File2FAssetDetail[strReourceNameDependence].mAssetType, lastpakNum);
					if (GetResourceTypeMustInApk(m_File2FAssetDetail[strReourceNameDependence].mAssetType))
					{
						PakName = FString::Printf(TEXT("%s_%s_%d_APK"), *m_strPlatform, *m_File2FAssetDetail[strReourceNameDependence].mAssetType, lastpakNum);
					}
					m_TypePakNum[m_File2FAssetDetail[strReourceNameDependence].mAssetType].Add(lastpakNum);
				}
				else
				{
					if (isPakFull(strReourceNameDependence, PakName, m_File2FAssetDetail[strReourceNameDependence].mAssetType))
					{
						lastpakNum = lastpakNum + 1;
						PakName = FString::Printf(TEXT("%s_%s_%d_DLC"), *m_strPlatform, *m_File2FAssetDetail[strReourceNameDependence].mAssetType, lastpakNum);
						if (GetResourceTypeMustInApk(m_File2FAssetDetail[strReourceNameDependence].mAssetType))
						{
							PakName = FString::Printf(TEXT("%s_%s_%d_APK"), *m_strPlatform, *m_File2FAssetDetail[strReourceNameDependence].mAssetType, lastpakNum);
						}
						m_TypePakNum[m_File2FAssetDetail[strReourceNameDependence].mAssetType].Add(lastpakNum);
					}
				}
				//记录场景名
				lastScene = Item.Key;
				AssetResoruceFillinPak(strReourceNameDependence, PakName, SceneName);
			}
		}
	}

	//增加Temp文件管理
	FillTempFilesinPak();

	FillExternFilesInPak();

	FillSceneDic();

	FillPakID2AllPakFileCommondsDic();

	ExecuteUnrealPakProcess();

	return bResult;
}

bool SHotPatcherCreatePaks::FillPakID2AllPakFileCommondsDic()
{
	bool bResult = false;

	FString ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	TArray<FString> CookedAssetAbsPath;
	TArray<FString> CookedAssetRelativePath;
	TArray<FString> FinalCookedCommand;
	TArray<FString> InCookParams;

	InCookParams.Add("-compress");

	for (const auto& ItemPakID2FileList : m_PakID2FileList)
	{
		for (auto& InAssetLongPackageName : ItemPakID2FileList.Value)
		{
			CookedAssetAbsPath.Empty();
			CookedAssetRelativePath.Empty();
			FinalCookedCommand.Empty();

			FString AssetCookedRelativePath = InAssetLongPackageName;

			// remove .uasset / .umap postfix
			{
				int32 lastDotIndex = 0;
				AssetCookedRelativePath.FindLastChar('.', lastDotIndex);
				AssetCookedRelativePath.RemoveAt(lastDotIndex, AssetCookedRelativePath.Len() - lastDotIndex);
			}

			if (UFLibAssetManageHelperEx::ConvLongPackageNameToCookedPath(ProjectDir, m_strPlatform, AssetCookedRelativePath, CookedAssetAbsPath, CookedAssetRelativePath))
			{
				UFLibAssetManageHelperEx::CombineCookedAssetCommand(CookedAssetAbsPath, CookedAssetRelativePath, InCookParams, FinalCookedCommand);

				for (auto itemCookedAssetAbsPath : CookedAssetAbsPath)
				{
					TMap<FString, FString> tempDic;
					if (m_AllPakFileCommandsFromPakListLogs.Contains(itemCookedAssetAbsPath))
					{
						if (!m_PakID2AllPakFileCommondsDic.Contains(ItemPakID2FileList.Key))
						{
							tempDic.Empty();
							tempDic.Add(itemCookedAssetAbsPath, m_AllPakFileCommandsFromPakListLogs[itemCookedAssetAbsPath]);
							m_PakID2AllPakFileCommondsDic.Add(ItemPakID2FileList.Key, tempDic);
						}
						else
						{
							if (!(m_PakID2AllPakFileCommondsDic[ItemPakID2FileList.Key]).Contains(itemCookedAssetAbsPath))
							{
								(m_PakID2AllPakFileCommondsDic[ItemPakID2FileList.Key]).Add(itemCookedAssetAbsPath, m_AllPakFileCommandsFromPakListLogs[itemCookedAssetAbsPath]);
							}
						}
					}
					else
					{
						UE_LOG(LogTemp, Log, TEXT("=======================SplitPaks NOT NOT NOT NOT in PAK : %s."), *itemCookedAssetAbsPath);
						//for (auto icmd : FinalCookedCommand)
						//{
						//	if (icmd.Contains(itemCookedAssetAbsPath))
						//	{
						//		if (!m_PakID2AllFileList.Contains(ItemPakID2FileList.Key))
						//		{
						//			tempDic.Empty();
						//			tempDic.Add(itemCookedAssetAbsPath, icmd);
						//			m_PakID2AllFileList.Add(ItemPakID2FileList.Key, tempDic);
						//		}
						//		else
						//		{
						//			if (!(m_PakID2AllFileList[ItemPakID2FileList.Key]).Contains(itemCookedAssetAbsPath))
						//			{
						//				(m_PakID2AllFileList[ItemPakID2FileList.Key]).Add(itemCookedAssetAbsPath, icmd);
						//				UE_LOG(LogTemp, Log, TEXT("=======================SplitPaks icmdicmdicmdicmdicmdicmd: %s."), *icmd);
						//			}
						//		}
						//	}
						// }
					}
				}
			}
		}
	}

	TArray<FString> FileListInPAK;
	for (auto itemPakID2AllFileDic : m_PakID2AllPakFileCommondsDic)
	{
		for (auto item : itemPakID2AllFileDic.Value)
		{
			FileListInPAK.Add(item.Key);
		}
	}

	TArray<FString> RelativePathList;
	m_TempFileRelativePath.GetKeys(RelativePathList);

	//EngineAndTemp

	for (auto itemFile : m_AllPakFileCommandsFromPakListLogs)
	{
		if (!FileListInPAK.Contains(itemFile.Key))
		{
			FString PakName = FString::Printf(TEXT("%s_EngineAndTemp_0_Apk"), *m_strPlatform);

			//添加额外文件分包判断
			FString RelativePath;
			FString DirectoryPath;
			if (TempFile2PakIsContains(itemFile.Value, RelativePathList, RelativePath))
			{
				if (m_TempFileRelativePath.Contains(RelativePath))
				{
					PakName = m_TempFileRelativePath[RelativePath];
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("=======================SplitPaks EngineAndTemp : m_TempFileRelativePath cant find RelativePath!!! RelativePath = %s"), *RelativePath);
					return false;
				}
			}
			else if(m_TempFile2Pak.Contains(itemFile.Key))
			{
				PakName = m_TempFile2Pak[itemFile.Key];
			}
			else if (m_ExternFiles2Pak.Contains(itemFile.Key))
			{
				PakName = m_ExternFiles2Pak[itemFile.Key];
				m_TempFileNoSpecify2Pak.Add(itemFile.Key, PakName);
			}
			else if (IsInExternDir(itemFile.Key, DirectoryPath))
			{
				PakName = m_ExternDirectory2Pak[DirectoryPath];
				m_TempFileNoSpecify2Pak.Add(itemFile.Key, PakName);
			}
			else
			{
				//非指定包文件，添加索引
				m_TempFileNoSpecify2Pak.Add(itemFile.Key, PakName);
			}
			
			FileListInPAK.Add(itemFile.Key);
			TMap<FString, FString> tempDic;
			if (!m_PakID2AllPakFileCommondsDic.Contains(PakName))
			{
				tempDic.Empty();
				tempDic.Add(itemFile.Key, itemFile.Value);
				m_PakID2AllPakFileCommondsDic.Add(PakName, tempDic);
			}
			else
			{
				if (!(m_PakID2AllPakFileCommondsDic[PakName]).Contains(itemFile.Key))
				{
					(m_PakID2AllPakFileCommondsDic[PakName]).Add(itemFile.Key, itemFile.Value);
				}
			}
		}
	}

	FString PakListsDir = FString::Printf(TEXT("%sBuildData/%s/%s/PakLists/"), *FPaths::ProjectDir(), *m_strPlatform, FApp::GetProjectName());
	CreateDir(PakListsDir);

	for (auto itemPakID2AllFileDic : m_PakID2AllPakFileCommondsDic)
	{
		FString SavePath = FString::Printf(TEXT("%sBuildData/%s/%s/PakLists/%s.txt"), *FPaths::ProjectDir(), *m_strPlatform, FApp::GetProjectName(), *itemPakID2AllFileDic.Key);
		TArray<FString> OutPakCommand;

		for (auto item : itemPakID2AllFileDic.Value)
		{
			OutPakCommand.Add(item.Value);
		}
		FFileHelper::SaveStringArrayToFile(OutPakCommand, *SavePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	if (FileListInPAK.Num() != m_AllPakFileCommandsFromPakListLogs.Num())
	{
		UE_LOG(LogTemp, Error, TEXT("=======================SplitPaks CookedPath : FileListInPAK.Num():%d m_AllPakFileCommandsFromPakListLogs.Num() %d "), FileListInPAK.Num(), m_AllPakFileCommandsFromPakListLogs.Num());
	}

	return bResult;
}

void SHotPatcherCreatePaks::CreateDir(FString filePath)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (PlatformFile.DirectoryExists(*filePath))
	{
		PlatformFile.DeleteDirectoryRecursively(*filePath);
		UE_LOG(LogTemp, Warning, TEXT("DeleteDirectoryRecursively: DeleteDirectoryRecursively  successfully! %s"), *filePath);
	}

	if (PlatformFile.CreateDirectory(*filePath))//这里不一样！
	{
		UE_LOG(LogTemp, Warning, TEXT("createDic: CreateDic  successfully! %s"), *filePath);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("createDic: Not CreateDic!"));
	}
}

bool SHotPatcherCreatePaks::ExecuteUnrealPakProcess()
{
	bool bResult = false;
	TMap<FString, TArray<FPakFileInfo>> PakFilesInfoMap;
	TMap<FString, TArray<FString>> PakFiles;

	FString PaksDir = FString::Printf(TEXT("%sStagedBuilds/%s/%s/Content/Paks/"), *FPaths::ProjectSavedDir(), *m_strPlatform, FApp::GetProjectName());
	CreateDir(PaksDir);
	FString DLCPaksDir = FString::Printf(TEXT("%sBuildData/%s/%s/Content/Paks/"), *FPaths::ProjectDir(), *m_strPlatform, FApp::GetProjectName());
	CreateDir(DLCPaksDir);
	FString PakLogsDir = FString::Printf(TEXT("%sBuildData/%s/%s/PakLogs/"), *FPaths::ProjectDir(), *m_strPlatform, FApp::GetProjectName());
	CreateDir(PakLogsDir);

	//复制Crypto.json文件到BuildData内，以防误删
	CopyCryptoFile(m_CryptoPath);

	bool isPakThreadRunOK = true;

	for (auto itemPakID2AllFileDic : m_PakID2AllPakFileCommondsDic)
	{
		FString InSavePath = FString::Printf(TEXT("%sBuildData/%s/%s/PakLists/%s.txt"), *FPaths::ProjectDir(), *m_strPlatform, FApp::GetProjectName(), *itemPakID2AllFileDic.Key);
		UE_LOG(LogTemp, Log, TEXT("=======================SplitPaks InSavePath: %s."), *InSavePath);

		FString strPlatform = m_strPlatform;
		FThread CurrentPakThread(*InSavePath, [strPlatform, InSavePath, itemPakID2AllFileDic, &PakFilesInfoMap, &PakFiles, &isPakThreadRunOK]()
			{
				FString PakName = FString::Printf(TEXT("%sStagedBuilds/%s/%s/Content/Paks/%s.pak"), *FPaths::ProjectSavedDir(), *strPlatform, FApp::GetProjectName(), *itemPakID2AllFileDic.Key);
				if (PakName.EndsWith("DLC.pak"))
				{
					PakName = FString::Printf(TEXT("%sBuildData/%s/%s/Content/Paks/%s.pak"), *FPaths::ProjectDir(), *strPlatform, FApp::GetProjectName(), *itemPakID2AllFileDic.Key);
				}

				UE_LOG(LogTemp, Log, TEXT("=======================SplitPaks PakName: %s."), *PakName);
				FString PakFileListName = FString::Printf(TEXT("%sBuildData/%s/%s/PakLists/%s.txt"), *FPaths::ProjectDir(), *strPlatform, FApp::GetProjectName(), *itemPakID2AllFileDic.Key);
				UE_LOG(LogTemp, Log, TEXT("=======================SplitPaks PakFileListName: %s."), *PakFileListName);
				FString PakLOGName = FString::Printf(TEXT("%sBuildData/%s/%s/PakLogs/%s.txt"), *FPaths::ProjectDir(), *strPlatform, FApp::GetProjectName(), *itemPakID2AllFileDic.Key);
				UE_LOG(LogTemp, Log, TEXT("=======================SplitPaks PakLOGName: %s."), *PakLOGName);
				FString CryptoName = FString::Printf(TEXT("%sCooked/%s/%s/Metadata/Crypto.json"), *FPaths::ProjectSavedDir(), *strPlatform, FApp::GetProjectName());
				if (!FPaths::FileExists(CryptoName))
				{
					CryptoName = FString::Printf(TEXT("%sBuildData/%s/%s/Crypto.json"), *FPaths::ProjectDir(), *strPlatform, FApp::GetProjectName());
					if (!FPaths::FileExists(CryptoName))
					{
						isPakThreadRunOK = false;
						return;
					}
				}

				UE_LOG(LogTemp, Log, TEXT("=======================SplitPaks CryptoName: %s."), *CryptoName);
				FString CookerOpenOrderName = FString::Printf(TEXT("%sBuild/%s/FileOpenOrder/CookerOpenOrder.log"), *FPaths::ProjectDir(), *strPlatform);
				UE_LOG(LogTemp, Log, TEXT("=======================SplitPaks CookerOpenOrderName: %s."), *CookerOpenOrderName);
				FString CommandLine;

				//if (strPlatform.Equals("WindowsNoEditor"))
				if (strPlatform.StartsWith("Windows"))
				{
					/// <summary>
					/// -encryptindex -sign -patchpaddingalign=2048 -platform=Windows -compressionformats= -multiprocess
					/// </summary>
					/// <param name="strPlatform"></param>
					/// <returns></returns>
					CommandLine = FString::Printf(
						TEXT("%s -create=\"%s\" -cryptokeys=\"%s\" -order=\"%s\" -patchpaddingalign=2048 -platform=Windows -compressionformats=Zlib -multiprocess -abslog=\"%s\""),
						*PakName, *PakFileListName, *CryptoName, *CookerOpenOrderName, *PakLOGName);
					UE_LOG(LogTemp, Log, TEXT("=======================SplitPaks CommandLine: %s."), *CommandLine);
				}

				//if (strPlatform.Equals("Android_ASTC"))
				if (strPlatform.StartsWith("Android"))
				{
					CommandLine = FString::Printf(
						//TEXT("%s -create=\"%s\" -cryptokeys=\"%s\" -order=\"%s\" -encryptindex -platform=Android -compressionformats= -multiprocess -abslog=\"%s\""),
						TEXT("%s -create=\"%s\" -cryptokeys=\"%s\" -order=\"%s\" -encryptindex  -compressionformats=Zlib -multiprocess -abslog=\"%s\""),
						*PakName, *PakFileListName, *CryptoName, *CookerOpenOrderName, *PakLOGName);
					UE_LOG(LogTemp, Log, TEXT("=======================SplitPaks CommandLine: %s."), *CommandLine);
				}

				ExecuteUnrealPak(*CommandLine);

				if (FPaths::FileExists(*PakName))
				{
					if (!PakFiles.Contains(strPlatform))
					{
						PakFiles.Add(strPlatform, TArray<FString>{PakName});
					}
					else
					{
						PakFiles.Find(strPlatform)->Add(PakName);
					}

					FText Msg = LOCTEXT("SavedPakFileMsg", "Successd to Package the patch as Pak.");
					UFlibHotPatcherEditorHelper::CreateSaveFileNotify(Msg, PakName);

					FPakFileInfo CurrentPakInfo;
					if (UFlibPatchParserHelper::GetPakFileInfo(PakName, CurrentPakInfo))
					{
						// CurrentPakInfo.PakVersion = CurrentPakVersion;
						if (!PakFilesInfoMap.Contains(strPlatform))
						{
							PakFilesInfoMap.Add(strPlatform, TArray<FPakFileInfo>{CurrentPakInfo});
						}
						else
						{
							PakFilesInfoMap.Find(strPlatform)->Add(CurrentPakInfo);
						}
					}
				}
			});
		CurrentPakThread.Run();
		if (!isPakThreadRunOK)
		{
			break;
		}
	}

	if (!isPakThreadRunOK)
	{
		SetInformationContent(TEXT("ERROR : Crypto.json is not Exist!!!"));
		SetInfomationContentVisibility(EVisibility::Visible);
		return bResult;
	}

	SaveVersionFile("1.0.0", "version.json", PakFiles);

	WritePakFilesInfoMapToJsons(PakFilesInfoMap);

	WriteFile2PakDictToJson("AllAsset2PakID.json");

	WriteFile2PakAssetsMapToJson("AllPakAssetsMap.json");

	WriteScene2PaksDictToJson("AllScene2PakIDs.json");

	WriteTypeResourceToJson("m_TypeResource.json");

	WriteTypePakNumToJson("m_TypePakNum.json");

	WriteTempFile2PakToJson("m_TempFile2Pak.json");

	WriteTempFileNoSpecify2PakToJson("m_TempFileNoSpecify2Pak.json");

	return bResult;
}

bool SHotPatcherCreatePaks::WriteFile2PakAssetsMapToJson(FString FileSaveName)
{
	TSharedPtr<FJsonObject> RootJsonObject = MakeShareable(new FJsonObject);

	for (const auto& pak : m_PakID2FileList)
	{
		TArray<FString> InArray;
		for (const auto& FilePathRef : pak.Value)
		{
			InArray.AddUnique(FilePathRef);
		}

		TArray<TSharedPtr<FJsonValue>> ArrayJsonValueList;
		for (const auto& ArrayItem : InArray)
		{
			ArrayJsonValueList.Add(MakeShareable(new FJsonValueString(ArrayItem)));
		}

		RootJsonObject->SetArrayField(pak.Key, ArrayJsonValueList);
	}

	FString OutString;
	auto JsonWriter = TJsonWriterFactory<TCHAR>::Create(&OutString);
	FJsonSerializer::Serialize(RootJsonObject.ToSharedRef(), JsonWriter);
	FString RecursiveDictToJson = FString::Printf(TEXT("%sBuildData/%s/%s/%s"), *FPaths::ProjectDir(), *m_strPlatform, FApp::GetProjectName(), *FileSaveName);
	if (UFLibAssetManageHelperEx::SaveStringToFile(RecursiveDictToJson, OutString))
	{
		auto Msg = LOCTEXT("RecursiveDictToJson", "Succeed to export Asset Related info.");
		UFlibHotPatcherEditorHelper::CreateSaveFileNotify(Msg, RecursiveDictToJson);
	}
	return true;
}

bool SHotPatcherCreatePaks::WritePakFilesInfoMapToJsons(TMap<FString, TArray<FPakFileInfo>>& PakFilesInfoMap)
{
	bool bRunStatus = false;
	FString OutString;
	TSharedPtr<FJsonObject> RootJsonObj;

	if (!RootJsonObj.IsValid())
	{
		RootJsonObj = MakeShareable(new FJsonObject);
	}

	// serialize
	{
		TArray<FString> PakPlatformKeys;
		PakFilesInfoMap.GetKeys(PakPlatformKeys);

		for (const auto& PakPlatformKey : PakPlatformKeys)
		{
			TSharedPtr<FJsonObject> CurrentPlatformPakJsonObj;
			TArray<TSharedPtr<FJsonValue>> CurrentPlatformPaksObjectList;
			for (const auto& Pak : *PakFilesInfoMap.Find(PakPlatformKey))
			{
				TSharedPtr<FJsonObject> CurrentPakJsonObj;
				//if (UFlibPatchParserHelper::SerializePakFileInfoToJsonObject(Pak, CurrentPakJsonObj))
				if (UFlibPatchParserHelper::TSerializeStructAsJsonObject(Pak, CurrentPakJsonObj))
				{
					CurrentPlatformPaksObjectList.Add(MakeShareable(new FJsonValueObject(CurrentPakJsonObj)));
				}
			}

			RootJsonObj->SetArrayField(PakPlatformKey, CurrentPlatformPaksObjectList);

			auto JsonWriter = TJsonWriterFactory<TCHAR>::Create(&OutString);
			FJsonSerializer::Serialize(RootJsonObj.ToSharedRef(), JsonWriter);
			FString PakFilesInfoMapToJson = FString::Printf(TEXT("%sBuildData/%s/%s/PakFilesInfoMap.json"), *FPaths::ProjectDir(), *PakPlatformKey, FApp::GetProjectName());
			if (UFLibAssetManageHelperEx::SaveStringToFile(PakFilesInfoMapToJson, OutString))
			{
				auto Msg = LOCTEXT("PakFilesInfoMapToJson", "Succeed to PakFilesInfoMapToJson info.");
				UFlibHotPatcherEditorHelper::CreateSaveFileNotify(Msg, PakFilesInfoMapToJson);
			}
			OutString = "";
			RootJsonObj = MakeShareable(new FJsonObject);
		}
		bRunStatus = true;
	}

	return bRunStatus;
}

bool SHotPatcherCreatePaks::GetAllPakFiles()
{
	bool bResult = false;

	TArray<FString> AllLogs;
	FString Extension = "PakList_*" + m_strPlatform + ".txt";
	ScanDirectoryRecursive(AllLogs, m_logsFilePath, Extension);

	for (auto item : AllLogs)
	{
		UE_LOG(LogTemp, Log, TEXT("=======================AllLogs : %s."), *item);
		TArray<FString> FileContents;
		FFileHelper::LoadFileToStringArray(FileContents, *item);
		for (auto itemPakFile : FileContents)
		{
			auto index = itemPakFile.Find("\" \"../../../");
			auto path = itemPakFile.Mid(1, index - 1);
			FPaths::NormalizeFilename(path);
			FPaths::NormalizeFilename(itemPakFile);
			UE_LOG(LogTemp, Log, TEXT("=======================m_AllPakFileList NormalizeFilename: %s. %s"), *path, *itemPakFile);
			m_AllPakFileCommandsFromPakListLogs.Add(path, itemPakFile);
		}
	}
	return bResult;
}

bool SHotPatcherCreatePaks::CanCreatePaks()const
{
	bool bCanExport = true;
	return bCanExport;
}

void SHotPatcherCreatePaks::CleanData()
{
	m_AllPak0Assets.Empty();

	m_TypePakNum.Empty();
	m_TypeCountSize.Empty();
	m_PakID2AllPakFileCommondsDic.Empty();

	m_AllAsset2PakID.Empty();
	m_PakID2FileList.Empty();
	m_Scene2FileList.Empty();
	m_Scene2PakIDList.Empty();

	m_AllPakFileCommandsFromPakListLogs.Empty();

	m_File2FAssetDetail.Empty();

	m_TempFile2Pak.Empty();
	m_TempFileRelativePath.Empty();

	m_TempFileNoSpecify2Pak.Empty();

	m_Paks_Item_List.Empty();

	m_ExternFiles2Pak.Empty();
	m_ExternDirectory2Pak.Empty();
}

void SHotPatcherCreatePaks::SaveVersionFile(FString versioninput, FString file, TMap<FString, TArray<FString>> PakFiles)
{
	TArray<FString> PakPlatformKeys;
	PakFiles.GetKeys(PakPlatformKeys);

	TSharedPtr<FJsonObject> rootObj = MakeShareable(new FJsonObject());
	FString version = versioninput;
	rootObj->SetStringField("Version", version);
	TArray<TSharedPtr<FJsonValue>> Paks;
	if (!PakFiles.Contains(m_strPlatform))
	{
		return;
	}
	for (const auto& Pak : *PakFiles.Find(m_strPlatform))
	{
		TSharedPtr<FJsonObject> t_insideObj = MakeShareable(new FJsonObject);

		FString PathPart;
		FString FileNamePart;
		FString ExtensionPart;
		uint32 FileCRC;
		uint32 FileType = 0;
		bool Downloaded = false;
		FPaths::Split(Pak, PathPart, FileNamePart, ExtensionPart);

		auto FileName = FString::Printf(TEXT("%s.%s"), *FileNamePart, *ExtensionPart);
		auto FileSize = IFileManager::Get().FileSize(*Pak);
		CalculateCRC(FileCRC, *Pak);

		if (Pak.EndsWith("DLC.pak"))
		{
			FileType = 2;
		}
		if (Pak.EndsWith("APK.pak"))
		{
			Downloaded = true;
		}

		t_insideObj->SetStringField("Pak", FileName);
		t_insideObj->SetNumberField("CRC", FileCRC);
		t_insideObj->SetNumberField("Type", FileType);
		t_insideObj->SetBoolField("Downloaded", Downloaded);
		t_insideObj->SetNumberField("Size", FileSize);
		TSharedPtr<FJsonValueObject> tmp = MakeShareable(new FJsonValueObject(t_insideObj));
		Paks.Add(tmp);
	}
	rootObj->SetArrayField("Paks", Paks);
	FString jsonStr;
	TSharedRef<TJsonWriter<TCHAR>> jsonWriter = TJsonWriterFactory<TCHAR>::Create(&jsonStr);
	FJsonSerializer::Serialize(rootObj.ToSharedRef(), jsonWriter);

	FString PakFilesInfoMapToJson = FString::Printf(TEXT("%sBuildData/%s/%s/Content/Paks/%s"), *FPaths::ProjectDir(), *m_strPlatform, FApp::GetProjectName(), *file);
	if (UFLibAssetManageHelperEx::SaveStringToFile(PakFilesInfoMapToJson, jsonStr))
	{
		auto Msg = LOCTEXT("PakFilesInfoMapToJson", "Succeed to PakFilesInfoMapToJson info.");
		UFlibHotPatcherEditorHelper::CreateSaveFileNotify(Msg, PakFilesInfoMapToJson);
	}
}

void SHotPatcherCreatePaks::ChangeVersionFile(FString file, TMap<FString, TArray<FString>> PakFiles)
{
	TArray<FString> PakPlatformKeys;
	PakFiles.GetKeys(PakPlatformKeys);

	TSharedPtr<FJsonObject> rootObj = MakeShareable(new FJsonObject());
	FString version = m_CreatePaksSettings->VersionId;
	rootObj->SetStringField("Version", version);
	//TArray<TSharedPtr<FJsonValue>> Paks;
	if (!PakFiles.Contains(m_strPlatform))
	{
		return;
	}

	//FString PakFilesInfoMapToJson = FString::Printf(TEXT("%sBuildData/%s/%s/Content/Paks/%s"), *FPaths::ProjectDir(), *m_strPlatform, FApp::GetProjectName(), *file);
	FString PakFilesInfoMapToJson = FString::Printf(TEXT("%s/%s/%s/Content/Paks/%s"), *m_CreatePaksSettings->GetBuildDataPath(), *m_strPlatform, FApp::GetProjectName(), *file);

	if (!m_Paks_Item_List.Num())
	{
		LoadVersionJsonFile(PakFilesInfoMapToJson);
	}

	for (const auto& Pak : *PakFiles.Find(m_strPlatform))
	{
		TSharedPtr<FJsonObject> t_insideObj = MakeShareable(new FJsonObject);

		FString PathPart;
		FString FileNamePart;
		FString ExtensionPart;
		uint32 FileCRC;
		uint32 FileType = 0;
		bool Downloaded = false;
		FPaths::Split(Pak, PathPart, FileNamePart, ExtensionPart);

		auto FileName = FString::Printf(TEXT("%s.%s"), *FileNamePart, *ExtensionPart);
		auto FileSize = IFileManager::Get().FileSize(*Pak);
		CalculateCRC(FileCRC, *Pak);

		if (Pak.EndsWith("DLC.pak") || Pak.EndsWith("_P.pak"))
		{
			FileType = 2;
		}
		if (Pak.EndsWith("APK.pak"))
		{
			Downloaded = true;
		}

		t_insideObj->SetStringField("Pak", FileName);
		t_insideObj->SetNumberField("CRC", FileCRC);
		t_insideObj->SetNumberField("Type", FileType);
		t_insideObj->SetBoolField("Downloaded", Downloaded);
		t_insideObj->SetNumberField("Size", FileSize);
		TSharedPtr<FJsonValueObject> tmp = MakeShareable(new FJsonValueObject(t_insideObj));

		//查找原先存在的数据，进行更新，不存在则新增
		int32 existIndex = -1;
		for (int32 Index = 0; Index != m_Paks_Item_List.Num(); ++Index)
		{
			TSharedPtr<FJsonValue> pak_item = m_Paks_Item_List[Index];
			TSharedPtr<FJsonObject> pak_item_obj;
			pak_item_obj = pak_item->AsObject();
			if (pak_item_obj->GetStringField("Pak") == t_insideObj->GetStringField("Pak"))
			{
				existIndex = Index;
			}
		}

		if (existIndex != -1)
		{
			m_Paks_Item_List[existIndex] = tmp;
		}
		else
		{
			m_Paks_Item_List.Add(tmp);
		}
	}
	rootObj->SetArrayField("Paks", m_Paks_Item_List);
	FString jsonStr;
	TSharedRef<TJsonWriter<TCHAR>> jsonWriter = TJsonWriterFactory<TCHAR>::Create(&jsonStr);
	FJsonSerializer::Serialize(rootObj.ToSharedRef(), jsonWriter);

	if (UFLibAssetManageHelperEx::SaveStringToFile(PakFilesInfoMapToJson, jsonStr))
	{
		auto Msg = LOCTEXT("PakFilesInfoMapToJson", "Succeed to PakFilesInfoMapToJson info.");
		UFlibHotPatcherEditorHelper::CreateSaveFileNotify(Msg, PakFilesInfoMapToJson);
	}
}

bool SHotPatcherCreatePaks::CalculateCRC(uint32& crcValue, FString path)
{
	FArchive* Ar = IFileManager::Get().CreateFileReader(*path);
	if (Ar)
	{
		TArray<uint8> LocalScratch;
		TArray<uint8>* Buffer = nullptr;
		if (!Buffer)
		{
			LocalScratch.SetNumUninitialized(1024 * 64);
			Buffer = &LocalScratch; //-V506
		}

		uint32 value = 0;
		FCrc crc;
		crc.Init();

		const int64 Size = Ar->TotalSize();
		int64 Position = 0;

		// Read in BufferSize chunks
		while (Position < Size)
		{
			const auto ReadNum = FMath::Min(Size - Position, (int64)Buffer->Num());
			Ar->Serialize(Buffer->GetData(), ReadNum);
			value = crc.MemCrc32(Buffer->GetData(), ReadNum, value);
			Position += ReadNum;
		}
		crcValue = value;
		return true;
	}
	else
	{
		UE_LOG(LogStreaming, Warning, TEXT("Failed to read file '%s' error."), *path);
		return false;
	}
}

FReply SHotPatcherCreatePaks::DoCreatePaks()
{
	SetInformationContent(TEXT(""));
	SetInfomationContentVisibility(EVisibility::Collapsed);

	float AmountOfWorkProgress = 4.0f;
	UE_LOG(LogTemp, Log, TEXT("=======================KDLCPatcher is DoCreatePaks."));

	TArray<FString> InFilterPackagePaths = TArray<FString>{ "/Game" };
	TArray<EAssetRegistryDependencyTypeEx> AssetRegistryDependencyTypes = m_CreatePaksSettings->GetAssetRegistryDependencyTypes();//TArray<EAssetRegistryDependencyTypeEx>{ EAssetRegistryDependencyTypeEx::All };
	m_AllAssets.Empty();
	UFLibAssetManageHelperEx::GetAssetsList(InFilterPackagePaths, AssetRegistryDependencyTypes, m_AllAssets);
	auto AnalysisAssetDependency = [](const TArray<FAssetDetail>& InAssetDetail, const TArray<EAssetRegistryDependencyTypeEx>& AssetRegistryDependencyTypes, bool bInAnalysisDepend)->FAssetDependenciesInfo
	{
		FAssetDependenciesInfo RetAssetDepend;
		if (InAssetDetail.Num())
		{
			UFLibAssetManageHelperEx::CombineAssetsDetailAsFAssetDepenInfo(InAssetDetail, RetAssetDepend);

			if (bInAnalysisDepend)
			{
				FAssetDependenciesInfo AssetDependencies;
				UFLibAssetManageHelperEx::GetAssetListDependenciesForAssetDetail(InAssetDetail, AssetRegistryDependencyTypes, AssetDependencies);

				RetAssetDepend = UFLibAssetManageHelperEx::CombineAssetDependencies(RetAssetDepend, AssetDependencies);
			}
		}
		return RetAssetDepend;
	};
	// 分析过滤器中指定的资源依赖
	FAssetDependenciesInfo FilterAssetDependencies = AnalysisAssetDependency(m_AllAssets, AssetRegistryDependencyTypes, true);
	m_AssetsDependency = UFlibPatchParserHelper::GetAssetsRelatedInfoByFAssetDependencies(FilterAssetDependencies, AssetRegistryDependencyTypes);
	//分apk和dlc场景
	FillTypeResourceSilptDLCandAPKLevel();

	TArray<FString> AllPlatforms;
	FString CheckResult;
	for (const auto& Platform : m_CreatePaksSettings->GetPakTargetPlatforms())
	{
		CleanData();

		AllPlatforms.AddUnique(UFlibPatchParserHelper::GetEnumNameByValue(Platform));

		m_logsFilePath = m_CreatePaksSettings->GetLogsFilePath();
		m_strPlatform = UFlibPatchParserHelper::GetEnumNameByValue(Platform);//m_strPlatform;//"WindowsNoEditor";
		UE_LOG(LogTemp, Log, TEXT("#############m_logsFilePath : %s."), *m_logsFilePath);
		UE_LOG(LogTemp, Log, TEXT("#############m_strPlatform : %s."), *m_strPlatform);

		//检查生成包需要的文件是否存在
		if (!SHotPatcherCreatePaks::CheckCommandNeedFiles(CheckResult, m_CryptoPath))
		{
			SetInformationContent(CheckResult);
			SetInfomationContentVisibility(EVisibility::Visible);
			return FReply::Unhandled();
		}

		FString BuildData = FString::Printf(TEXT("%sBuildData/%s"), *FPaths::ProjectDir(), *m_strPlatform);
		CreateDir(BuildData);

		FString AssetsDependencyString;
		//UFlibPatchParserHelper::SerializeAssetsRelatedInfoAsString(m_AssetsDependency, AssetsDependencyString);
		AssetsDependencyString = UFlibPatchParserHelper::SerializeAssetsDependencyAsJsonString(m_AssetsDependency);
		FString SaveAssetRelatedInfoToFile = FString::Printf(TEXT("%s/%s/Pacakage_AssetRelatedInfos.json"), *BuildData, FApp::GetProjectName());
		if (UFLibAssetManageHelperEx::SaveStringToFile(SaveAssetRelatedInfoToFile, AssetsDependencyString))
		{
			auto Msg = LOCTEXT("SaveAssetRelatedInfoInfo", "Succeed to export Asset Related info.");
			UFlibHotPatcherEditorHelper::CreateSaveFileNotify(Msg, SaveAssetRelatedInfoToFile);
		}

		FillDirectAndRecursionDependence();

		GetAllPakFiles();
		//SplitPaksbyAPKandDLC(strPlatform);
		SpiltPakByResourceType();
	}

	return FReply::Handled();
}

bool SHotPatcherCreatePaks::CanDiff()const
{
	bool bCanDiff = false;
	if (GPatchSettings)
	{
		bool bHasBase = !GPatchSettings->GetBaseVersion().IsEmpty() && FPaths::FileExists(GPatchSettings->GetBaseVersion());
		bool bHasVersionId = !GPatchSettings->GetVersionId().IsEmpty();
		bool bHasFilter = !!GPatchSettings->GetAssetIncludeFiltersPaths().Num();
		bool bHasSpecifyAssets = !!GPatchSettings->GetIncludeSpecifyAssets().Num();

		bCanDiff = bHasBase && bHasVersionId && (bHasFilter || bHasSpecifyAssets);
	}
	return bCanDiff;
}
FReply SHotPatcherCreatePaks::DoDiff()const
{
	FString BaseVersionContent;
	FHotPatcherVersion BaseVersion;

	bool bDeserializeStatus = false;

	if (GPatchSettings->IsByBaseVersion())
	{
		if (UFLibAssetManageHelperEx::LoadFileToString(GPatchSettings->GetBaseVersion(), BaseVersionContent))
		{
			bDeserializeStatus = UFlibPatchParserHelper::TDeserializeJsonStringAsStruct(BaseVersionContent, BaseVersion);
		}
		if (!bDeserializeStatus)
		{
			UE_LOG(LogHotPatcher, Error, TEXT("Deserialize Base Version Faild!"));
			return FReply::Handled();
		}
	}

	FHotPatcherVersion CurrentVersion = UFlibPatchParserHelper::ExportReleaseVersionInfo(
		GPatchSettings->GetVersionId(),
		BaseVersion.VersionId,
		FDateTime::UtcNow().ToString(),
		GPatchSettings->GetAssetIncludeFiltersPaths(),
		GPatchSettings->GetAssetIgnoreFiltersPaths(),
		GPatchSettings->GetAssetRegistryDependencyTypes(),
		GPatchSettings->GetIncludeSpecifyAssets(),
		GPatchSettings->GetAddExternAssetsToPlatform(),
		GPatchSettings->IsIncludeHasRefAssetsOnly()
	);

	FPatchVersionDiff VersionDiffInfo = UFlibPatchParserHelper::DiffPatchVersionWithPatchSetting(*GPatchSettings, BaseVersion, CurrentVersion);

	bool bShowDeleteAsset = false;
	FString SerializeDiffInfo;
	UFlibPatchParserHelper::TSerializeStructAsJsonString(VersionDiffInfo, SerializeDiffInfo);
	SetInformationContent(SerializeDiffInfo);
	SetInfomationContentVisibility(EVisibility::Visible);

	return FReply::Handled();
}

EVisibility SHotPatcherCreatePaks::VisibilityDiffButtons() const
{
	bool bHasBase = false;
	if (GPatchSettings && GPatchSettings->IsByBaseVersion())
	{
		FString BaseVersionFile = GPatchSettings->GetBaseVersion();
		bHasBase = !BaseVersionFile.IsEmpty() && FPaths::FileExists(BaseVersionFile);
	}

	if (bHasBase && CanExportPatch())
	{
		return EVisibility::Visible;
	}
	else {
		return EVisibility::Collapsed;
	}
}

void SHotPatcherCreatePaks::SetInformationContent(const FString& InContent)const
{
	DiffWidget->SetContent(InContent);
}

void SHotPatcherCreatePaks::SetInfomationContentVisibility(EVisibility InVisibility)const
{
	DiffWidget->SetVisibility(InVisibility);
}

bool SHotPatcherCreatePaks::CanExportPatch()const
{
	bool bCanExport = false;
	if (GPatchSettings)
	{
		bool bHasBase = false;
		if (GPatchSettings->IsByBaseVersion())
			bHasBase = !GPatchSettings->GetBaseVersion().IsEmpty() && FPaths::FileExists(GPatchSettings->GetBaseVersion());
		else
			bHasBase = true;
		bool bHasVersionId = !GPatchSettings->GetVersionId().IsEmpty();
		bool bHasFilter = !!GPatchSettings->GetAssetIncludeFiltersPaths().Num();
		bool bHasSpecifyAssets = !!GPatchSettings->GetIncludeSpecifyAssets().Num();
		bool bHasExternFiles = !!GPatchSettings->GetAddExternFiles().Num();
		bool bHasExDirs = !!GPatchSettings->GetAddExternDirectory().Num();
		bool bHasSavePath = !GPatchSettings->GetSaveAbsPath().IsEmpty();
		bool bHasPakPlatfotm = !!GPatchSettings->GetPakTargetPlatforms().Num();

		bool bHasAnyPakFiles = (
			bHasFilter || bHasSpecifyAssets || bHasExternFiles || bHasExDirs ||
			GPatchSettings->IsIncludeEngineIni() ||
			GPatchSettings->IsIncludePluginIni() ||
			GPatchSettings->IsIncludeProjectIni()
			);
		bCanExport = bHasBase && bHasVersionId && bHasAnyPakFiles && bHasPakPlatfotm && bHasSavePath;
	}
	return bCanExport;
}

FReply SHotPatcherCreatePaks::DoCreateDiffPaks()
{
	SetInformationContent(TEXT(""));
	SetInfomationContentVisibility(EVisibility::Collapsed);

	FHotPatcherVersion BaseVersion;

	if (GetSettingObject()->IsByBaseVersion() && !GetSettingObject()->GetBaseVersionInfo(BaseVersion))
	{
		UE_LOG(LogHotPatcher, Error, TEXT("Deserialize Base Version Faild!"));
		return FReply::Unhandled();
	}
	else
	{
		// 在不进行外部文件diff的情况下清理掉基础版本的外部文件
		if (!GetSettingObject()->IsEnableExternFilesDiff())
		{
			BaseVersion.PlatformAssets.Empty();
		}
	}

	UFLibAssetManageHelperEx::UpdateAssetMangerDatabase(true);
	FChunkInfo NewVersionChunk = UFlibHotPatcherEditorHelper::MakeChunkFromPatchSettings(GetSettingObject());

	FHotPatcherVersion CurrentVersion = UFlibPatchParserHelper::ExportReleaseVersionInfoByChunk(
		GetSettingObject()->GetVersionId(),
		BaseVersion.VersionId,
		FDateTime::UtcNow().ToString(),
		NewVersionChunk,
		GetSettingObject()->IsIncludeHasRefAssetsOnly(),
		GetSettingObject()->IsAnalysisFilterDependencies()
	);

	FString CurrentVersionSavePath = GetSettingObject()->GetCurrentVersionSavePath();
	FPatchVersionDiff VersionDiffInfo = UFlibPatchParserHelper::DiffPatchVersionWithPatchSetting(*GetSettingObject(), BaseVersion, CurrentVersion);

	UPatcherProxy* PatcherProxy = NewObject<UPatcherProxy>();
	PatcherProxy->AddToRoot();
	PatcherProxy->SetProxySettings(GetSettingObject());

	//判断资源是否cook
	FString ReceiveMsg;
	if (!PatcherProxy->CheckPatchRequire(VersionDiffInfo, ReceiveMsg))
	{
		SetInformationContent(ReceiveMsg);
		SetInfomationContentVisibility(EVisibility::Visible);
		return FReply::Unhandled();
	}

	TArray<FPakCommand> AdditionalFileToPak;

	// save pakversion.json
	{
		FPakVersion CurrentPakVersion;
		if (PatcherProxy->SavePatchVersionJson(CurrentVersion, CurrentVersionSavePath, CurrentPakVersion))
		{
			FPakCommand VersionCmd;
			FString AbsPath = FExportPatchSettings::GetSavePakVersionPath(CurrentVersionSavePath, CurrentVersion);
			FString MountPath = GetSettingObject()->GetPakVersionFileMountPoint();
			VersionCmd.MountPath = MountPath;
			VersionCmd.PakCommands = TArray<FString>{
				FString::Printf(TEXT("\"%s\" \"%s\""),*AbsPath,*MountPath)
			};
			VersionCmd.AssetPackage = UFlibPatchParserHelper::MountPathToRelativePath(MountPath);
			AdditionalFileToPak.AddUnique(VersionCmd);
		}
	}

	{
		TArray<FString> DependenciesFilters;

		auto GetKeysLambda = [&DependenciesFilters](const FAssetDependenciesInfo& Assets)
		{
			TArray<FAssetDetail> AllAssets;
			UFLibAssetManageHelperEx::GetAssetDetailsByAssetDependenciesInfo(Assets, AllAssets);
			for (const auto& Asset : AllAssets)
			{
				FString Path;
				FString Filename;
				FString Extension;
				FPaths::Split(Asset.mPackagePath, Path, Filename, Extension);
				DependenciesFilters.AddUnique(Path);
			}
		};
		GetKeysLambda(VersionDiffInfo.AssetDiffInfo.AddAssetDependInfo);
		GetKeysLambda(VersionDiffInfo.AssetDiffInfo.ModifyAssetDependInfo);

		TArray<FDirectoryPath> DepOtherModule;

		for (const auto& DependenciesFilter : DependenciesFilters)
		{
			if (!!NewVersionChunk.AssetIncludeFilters.Num())
			{
				for (const auto& includeFilter : NewVersionChunk.AssetIncludeFilters)
				{
					if (!includeFilter.Path.StartsWith(DependenciesFilter))
					{
						FDirectoryPath FilterPath;
						FilterPath.Path = DependenciesFilter;
						DepOtherModule.Add(FilterPath);
					}
				}
			}
			else
			{
				FDirectoryPath FilterPath;
				FilterPath.Path = DependenciesFilter;
				DepOtherModule.Add(FilterPath);

			}
		}
		NewVersionChunk.AssetIncludeFilters.Append(DepOtherModule);
	}

	TArray<FString> PackagePathList;
	auto GetKeysLambda = [&PackagePathList](const FAssetDependenciesInfo& Assets)
	{
		TArray<FAssetDetail> AllAssets;
		UFLibAssetManageHelperEx::GetAssetDetailsByAssetDependenciesInfo(Assets, AllAssets);
		for (const auto& Asset : AllAssets)
		{
			//FString Path;
			//FString Filename;
			//FString Extension;
			//FPaths::Split(Asset.mPackagePath, Path, Filename, Extension);
			PackagePathList.AddUnique(Asset.mPackagePath);
		}
	};
	//GetKeysLambda(VersionDiffInfo.AssetDiffInfo.AddAssetDependInfo);
	GetKeysLambda(VersionDiffInfo.AssetDiffInfo.ModifyAssetDependInfo);

	//ModifyAssetDependInfo使用完，清空
	VersionDiffInfo.AssetDiffInfo.ModifyAssetDependInfo.AssetsDependenciesMap.Empty();


	//加载场景分类
	TArray<FString> InFilterPackagePaths = TArray<FString>{ "/Game" };
	TArray<EAssetRegistryDependencyTypeEx> AssetRegistryDependencyTypes = m_CreatePaksSettings->GetAssetRegistryDependencyTypes();//TArray<EAssetRegistryDependencyTypeEx>{ EAssetRegistryDependencyTypeEx::All };
	m_AllAssets.Empty();
	UFLibAssetManageHelperEx::GetAssetsList(InFilterPackagePaths, AssetRegistryDependencyTypes, m_AllAssets);
	auto AnalysisAssetDependency = [](const TArray<FAssetDetail>& InAssetDetail, const TArray<EAssetRegistryDependencyTypeEx>& AssetRegistryDependencyTypes, bool bInAnalysisDepend)->FAssetDependenciesInfo
	{
		FAssetDependenciesInfo RetAssetDepend;
		if (InAssetDetail.Num())
		{
			UFLibAssetManageHelperEx::CombineAssetsDetailAsFAssetDepenInfo(InAssetDetail, RetAssetDepend);

			if (bInAnalysisDepend)
			{
				FAssetDependenciesInfo AssetDependencies;
				UFLibAssetManageHelperEx::GetAssetListDependenciesForAssetDetail(InAssetDetail, AssetRegistryDependencyTypes, AssetDependencies);

				RetAssetDepend = UFLibAssetManageHelperEx::CombineAssetDependencies(RetAssetDepend, AssetDependencies);
			}
		}
		return RetAssetDepend;
	};
	// 分析过滤器中指定的资源依赖
	FAssetDependenciesInfo FilterAssetDependencies = AnalysisAssetDependency(m_AllAssets, AssetRegistryDependencyTypes, true);
	m_AssetsDependency = UFlibPatchParserHelper::GetAssetsRelatedInfoByFAssetDependencies(FilterAssetDependencies, AssetRegistryDependencyTypes);
	//分apk和dlc场景
	FillTypeResourceSilptDLCandAPKLevel();


	for (const auto& Platform : m_CreatePaksSettings->GetPakTargetPlatforms())
	{
		CleanData();

		m_strPlatform = UFlibPatchParserHelper::GetEnumNameByValue(Platform);

		//获取文件要跟着平台的路径
		FString AllAsset2PakIDFilePath;
		FString VersionPath;
		FString CheckResult;
		FString TypeResourceFilePath;
		FString TypePakNumFilePath;
		FString TempFileNoSpecifyPath;

		//检查生成包需要的文件是否存在
		if (!SHotPatcherCreatePaks::CheckCommandNeedFiles(CheckResult, m_CryptoPath, VersionPath, AllAsset2PakIDFilePath, TypeResourceFilePath, TypePakNumFilePath, TempFileNoSpecifyPath))
		{
			SetInformationContent(CheckResult);
			SetInfomationContentVisibility(EVisibility::Visible);
			return FReply::Unhandled();
		}

		FillDirectAndRecursionDependence(true);

		//处理修改的文件打包
		if (!!PackagePathList.Num())
		{
			if (!GetAllAsset2PakInfo(PackagePathList, VersionPath, AllAsset2PakIDFilePath))
			{
				return FReply::Unhandled();
			}
		}

		//处理新增文件打包
		if (!DealAddAssetPak(VersionDiffInfo, Platform, NewVersionChunk, AdditionalFileToPak, AllAsset2PakIDFilePath, TypeResourceFilePath, TypePakNumFilePath, TempFileNoSpecifyPath))
		{
			return FReply::Unhandled();
		}

		//生成新的版本文件 TODO：目前先按照版本号生成文件，后续可以改为自动读取，加入版本管理
		FString SerializeReleaseVersionInfo;
		FHotPatcherVersion NewReleaseVersion = PatcherProxy->MakeNewRelease(BaseVersion, CurrentVersion, GetSettingObject());
		UFlibPatchParserHelper::TSerializeStructAsJsonString(NewReleaseVersion, SerializeReleaseVersionInfo);

		FString SaveCurrentVersionToFile = FString::Printf(TEXT("%s/%s/%s/%s_Release.json"), *m_CreatePaksSettings->GetBuildDataPath(), *m_strPlatform, FApp::GetProjectName(), *m_CreatePaksSettings->VersionId);
		UFLibAssetManageHelperEx::SaveStringToFile(SaveCurrentVersionToFile, SerializeReleaseVersionInfo);
	}

	return FReply::Handled();
}

bool SHotPatcherCreatePaks::CanCreateDiffPaks()const
{
	bool bCanExport = true;
	if (m_CreatePaksSettings->GetBuildDataPath().IsEmpty() || !CanDiff() || m_CreatePaksSettings->VersionId.IsEmpty())
	{
		bCanExport = false;
	}

	return bCanExport;
}

void SHotPatcherCreatePaks::LoadVersionJsonFile(FString& InVersionPath)
{
	FString VersionContent;
	//FString VersionOld;
	TSharedPtr<FJsonValue> Paks;

	FString moveVersionPath;

	if (UFLibAssetManageHelperEx::LoadFileToString(InVersionPath, VersionContent))
	{
		TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(VersionContent);
		TSharedPtr<FJsonObject> DeserializeJsonObject;
		FJsonSerializer::Deserialize(JsonReader, DeserializeJsonObject);
		DeserializeJsonObject->TryGetStringField(TEXT("Version"), m_VersionOld);
		Paks = DeserializeJsonObject->TryGetField(TEXT("Paks"));

		m_Paks_Item_List = Paks->AsArray();

		//根据版本号重命名
		int32 LastPahtEndIndex = InVersionPath.Find(TEXT("/"), ESearchCase::IgnoreCase, ESearchDir::FromEnd, -1);
		moveVersionPath = UKismetStringLibrary::GetSubstring(InVersionPath, 0, LastPahtEndIndex);

		moveVersionPath = moveVersionPath + "/version_" + m_VersionOld + ".json";

		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		if (PlatformFile.CopyFile(*moveVersionPath, *InVersionPath))
		{
			UE_LOG(LogTemp, Log, TEXT("=======================BackUpVersionFile: %s."), *moveVersionPath);
		}
	}
}

void SHotPatcherCreatePaks::LoadAllAsset2PakIDJsonFile(FString& InFilePath)
{
	FString FileContent;

	FString moveFilePath;

	if (UFLibAssetManageHelperEx::LoadFileToString(InFilePath, FileContent))
	{
		TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(FileContent);
		//TSharedPtr<FJsonObject> DeserializeJsonObject;
		//将解析出来的JsonObject存入m_AllAsset2PakID_JsonObj
		FJsonSerializer::Deserialize(JsonReader, m_AllAsset2PakID_JsonObj);
		
		//根据版本号重命名
		int32 LastPahtEndIndex = InFilePath.Find(TEXT("/"), ESearchCase::IgnoreCase, ESearchDir::FromEnd, -1);
		moveFilePath = UKismetStringLibrary::GetSubstring(InFilePath, 0, LastPahtEndIndex);

		moveFilePath = moveFilePath + "/AllAsset2PakID_" + m_VersionOld + ".json";

		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		if (PlatformFile.CopyFile(*moveFilePath, *InFilePath))
		{
			UE_LOG(LogTemp, Log, TEXT("=======================BackUpVersionFile: %s."), *moveFilePath);
		}
	}
}

void SHotPatcherCreatePaks::LoadTempFileNoSpecify2PakFile(FString& InFilePath)
{
	FString FileContent;

	FString moveFilePath;

	if (UFLibAssetManageHelperEx::LoadFileToString(InFilePath, FileContent))
	{
		TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(FileContent);
		TSharedPtr<FJsonObject> DeserializeJsonObject;
		//将解析出来的JsonObject存入m_TempFileNoSpecify2Pak_JsonObj(不采用)
		FJsonSerializer::Deserialize(JsonReader, DeserializeJsonObject);
		for (const auto& Item_TempFile : DeserializeJsonObject->Values)
		{
			m_TempFileNoSpecify2Pak.Add(Item_TempFile.Key, DeserializeJsonObject->GetStringField(Item_TempFile.Key));
		}

		//根据版本号重命名
		int32 LastPahtEndIndex = InFilePath.Find(TEXT("/"), ESearchCase::IgnoreCase, ESearchDir::FromEnd, -1);
		moveFilePath = UKismetStringLibrary::GetSubstring(InFilePath, 0, LastPahtEndIndex);

		moveFilePath = moveFilePath + "/TempFileNoSpecify2Pak_" + m_VersionOld + ".json";

		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		if (PlatformFile.CopyFile(*moveFilePath, *InFilePath))
		{
			UE_LOG(LogTemp, Log, TEXT("=======================BackUpVersionFile: %s."), *moveFilePath);
		}
	}
}


bool SHotPatcherCreatePaks::GetAllAsset2PakInfo(TArray<FString>& InPackagePathList,FString& VersionPath,FString& AllAsset2PakIDFilePath)
{
	FString AllAsset2PakContent;

	//加载并保存之前版本的version.json文件
	LoadVersionJsonFile(VersionPath);

	if (UFLibAssetManageHelperEx::LoadFileToString(AllAsset2PakIDFilePath, AllAsset2PakContent))
	{
		TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(AllAsset2PakContent);
		TSharedPtr<FJsonObject> DeserializeJsonObject;
		FJsonSerializer::Deserialize(JsonReader, DeserializeJsonObject);
		TArray<FString> PakNameList;
		FString PakName;
		//m_pakListsPath = m_CreatePaksSettings->GetPakListsPath();
		for (const auto& PackagePath : InPackagePathList)
		{
			if (DeserializeJsonObject->TryGetStringField(PackagePath, PakName))
			{
				PakNameList.AddUnique(PakName);
			}
		}

		if (!ExecuteUnrealPakProcess_ByDiff(PakNameList))
		{
			return false;
		}
	}

	return true;
}

bool SHotPatcherCreatePaks::ExecuteUnrealPakProcess_ByDiff(TArray<FString>& PakNameList,bool is_add)
{
	bool isPakThreadRunOK = true;

	TMap<FString, TArray<FPakFileInfo>> PakFilesInfoMap;
	TMap<FString, TArray<FString>> PakFiles;

	for (const auto& PakName : PakNameList)
	{
		FString strPlatform = m_strPlatform;

		FThread CurrentPakThread(*PakName, [strPlatform, &PakName, &PakFilesInfoMap, &PakFiles, &isPakThreadRunOK, this]()
			{
				FString CurPakName = FString::Printf(TEXT("%sStagedBuilds/%s/%s/Content/Paks/%s.pak"), *FPaths::ProjectSavedDir(), *strPlatform, FApp::GetProjectName(), *PakName);

				FString BuildDataPath = m_CreatePaksSettings->GetBuildDataPath();

				if (CurPakName.EndsWith("DLC.pak") || CurPakName.EndsWith("_P.pak"))
				{
					CurPakName = FString::Printf(TEXT("%s/%s/%s/Content/Paks/%s.pak"), *BuildDataPath, *strPlatform, FApp::GetProjectName(), *PakName);
				}
				else if (CurPakName.EndsWith("APK.pak"))
				{
					CurPakName = FString::Printf(TEXT("%s/%s/%s/Content/Paks/%s_001_P.pak"), *BuildDataPath, *strPlatform, FApp::GetProjectName(), *PakName);
				}

				UE_LOG(LogTemp, Log, TEXT("=======================SplitPaks PakName: %s."), *CurPakName);
				FString PakFileListName = FString::Printf(TEXT("%s/%s/%s/PakLists/%s.txt"), *BuildDataPath, *strPlatform, FApp::GetProjectName(), *PakName);
				UE_LOG(LogTemp, Log, TEXT("=======================SplitPaks PakFileListName: %s."), *PakFileListName);
				FString PakLOGName = FString::Printf(TEXT("%s/%s/%s/PakLogs/%s.txt"), *BuildDataPath, *strPlatform, FApp::GetProjectName(), *PakName);
				UE_LOG(LogTemp, Log, TEXT("=======================SplitPaks PakLOGName: %s."), *PakLOGName);
				FString CryptoName = FString::Printf(TEXT("%sCooked/%s/%s/Metadata/Crypto.json"), *FPaths::ProjectSavedDir(), *strPlatform, FApp::GetProjectName());

				if (!FPaths::FileExists(CryptoName))
				{
					CryptoName = FString::Printf(TEXT("%s/%s/%s/Crypto.json"), *BuildDataPath, *strPlatform, FApp::GetProjectName());
					if (!FPaths::FileExists(CryptoName))
					{
						isPakThreadRunOK = false;
						return;
					}
				}

				UE_LOG(LogTemp, Log, TEXT("=======================SplitPaks CryptoName: %s."), *CryptoName);
				FString CookerOpenOrderName = FString::Printf(TEXT("%sBuild/%s/FileOpenOrder/CookerOpenOrder.log"), *FPaths::ProjectDir(), *strPlatform);
				UE_LOG(LogTemp, Log, TEXT("=======================SplitPaks CookerOpenOrderName: %s."), *CookerOpenOrderName);
				FString CommandLine;

				//if (strPlatform.Equals("WindowsNoEditor"))
				if (strPlatform.StartsWith("Windows"))
				{
					/// <summary>
					/// -encryptindex -sign -patchpaddingalign=2048 -platform=Windows -compressionformats= -multiprocess
					/// </summary>
					/// <param name="strPlatform"></param>
					/// <returns></returns>
					CommandLine = FString::Printf(
						TEXT("%s -create=\"%s\" -cryptokeys=\"%s\" -order=\"%s\" -patchpaddingalign=2048 -platform=Windows -compressionformats=Zlib -multiprocess -abslog=\"%s\""),
						*CurPakName, *PakFileListName, *CryptoName, *CookerOpenOrderName, *PakLOGName);
					UE_LOG(LogTemp, Log, TEXT("=======================SplitPaks CommandLine: %s."), *CommandLine);
				}

				//if (strPlatform.Equals("Android_ASTC"))
				if (strPlatform.StartsWith("Android"))
				{
					CommandLine = FString::Printf(
						//TEXT("%s -create=\"%s\" -cryptokeys=\"%s\" -order=\"%s\" -encryptindex -platform=Android -compressionformats= -multiprocess -abslog=\"%s\""),
						TEXT("%s -create=\"%s\" -cryptokeys=\"%s\" -order=\"%s\" -encryptindex  -compressionformats=Zlib -multiprocess -abslog=\"%s\""),
						*CurPakName, *PakFileListName, *CryptoName, *CookerOpenOrderName, *PakLOGName);
					UE_LOG(LogTemp, Log, TEXT("=======================SplitPaks CommandLine: %s."), *CommandLine);
				}

				ExecuteUnrealPak(*CommandLine);

				if (FPaths::FileExists(*CurPakName))
				{
					if (!PakFiles.Contains(strPlatform))
					{
						PakFiles.Add(strPlatform, TArray<FString>{CurPakName});
					}
					else
					{
						PakFiles.Find(strPlatform)->Add(CurPakName);
					}

					FText Msg = LOCTEXT("SavedPakFileMsg", "Successd to Package the patch as Pak.");
					UFlibHotPatcherEditorHelper::CreateSaveFileNotify(Msg, CurPakName);

					//FPakFileInfo CurrentPakInfo;
					//if (UFlibPatchParserHelper::GetPakFileInfo(CurPakName, CurrentPakInfo))
					//{
					//	// CurrentPakInfo.PakVersion = CurrentPakVersion;
					//	if (!PakFilesInfoMap.Contains(strPlatform))
					//	{
					//		PakFilesInfoMap.Add(strPlatform, TArray<FPakFileInfo>{CurrentPakInfo});
					//	}
					//	else
					//	{
					//		PakFilesInfoMap.Find(strPlatform)->Add(CurrentPakInfo);
					//	}
					//}
				}
			});
		CurrentPakThread.Run();
		if (!isPakThreadRunOK)
		{
			break;
		}
	}

	if (!isPakThreadRunOK)
	{
		SetInformationContent(TEXT("ERROR : Crypto.json is not Exist!!!"));
		SetInfomationContentVisibility(EVisibility::Visible);
		return false;
	}

	ChangeVersionFile("version.json", PakFiles);

	if (is_add)
	{
		//使用m_AllAsset2PakID_JsonObj，写入AllAsset2PakID.json文件
		WriteAllAsset2PakIDByJsonObj("AllAsset2PakID.json");

		//写状态文件 TODO:先以覆盖形式，后续增加旧版本文件归档文件夹
		WriteTypeResourceToJson("m_TypeResource.json",true);

		WriteTypePakNumToJson("m_TypePakNum.json",true);

		WriteTempFileNoSpecify2PakToJson("m_TempFileNoSpecify2Pak.json",true);

		WriteScene2PaksDictToJson("AllScene2PakIDs.json",true);
	}

	return true;
}


bool SHotPatcherCreatePaks::CheckCommandNeedFiles(FString& OutCheckResult, FString& OutCryptoPath)
{
	OutCheckResult.Empty();
	FString strPlatform = m_strPlatform;
	FString CryptoName = FString::Printf(TEXT("%sCooked/%s/%s/Metadata/Crypto.json"), *FPaths::ProjectSavedDir(), *strPlatform, FApp::GetProjectName());

	OutCryptoPath = *CryptoName;

	//FString CookerOpenOrderName = FString::Printf(TEXT("%sBuild/%s/FileOpenOrder/CookerOpenOrder.log"), *FPaths::ProjectDir(), *strPlatform);

	bool bHasCrypto = true;
	bool bHasCookerOpenOrder = true;

	if (!FPaths::FileExists(*CryptoName))
	{
		OutCheckResult.Append(FString::Printf(TEXT("\t%s is not exist!!\n"), *CryptoName));
		bHasCrypto = false;
	}

	//if (!FPaths::FileExists(*CookerOpenOrderName))
	//{
	//	OutCheckResult.Append(FString::Printf(TEXT("\t%s is not exist!!\n"), *CookerOpenOrderName));
	//	bHasCookerOpenOrder = false;
	//}

	return bHasCrypto && bHasCookerOpenOrder;
}

bool SHotPatcherCreatePaks::CheckCommandNeedFiles(FString& OutCheckResult, FString& OutCryptoPath, FString& OutVersionPath, FString& OutAllAsset2PakIDFilePath, FString& OutTypeResourceFilePath, FString& OutTypePakNumFilePath, FString& OutTempFileNoSpecifyPath)
{
	OutCheckResult.Empty();
	FString strPlatform = m_strPlatform;

	bool bHasVersionJson = true;
	bool bHasCrypto = true;
	bool bHasCookerOpenOrder = true;
	bool bHasAllAsset2PakIDFile = true;
	bool bHasTypeResourceFile = true;
	bool bHasTypePakNumFile = true;
	bool bHasTempFileNoSpecifyFile = true;

	bool bHasDirPakLists = true;
	bool bHasDirPaks = true;

	FString BuildDataPath = m_CreatePaksSettings->GetBuildDataPath();

	FString CryptoName = FString::Printf(TEXT("%s/%s/%s/Crypto.json"), *BuildDataPath, *strPlatform, FApp::GetProjectName());

	OutCryptoPath = *CryptoName;

	FString VersionJsonPath = FString::Printf(TEXT("%s/%s/%s/Content/Paks/version.json"), *BuildDataPath, *strPlatform, FApp::GetProjectName());
	//FString CookerOpenOrderName = FString::Printf(TEXT("%sBuild/%s/FileOpenOrder/CookerOpenOrder.log"), *FPaths::ProjectDir(), *strPlatform);
	OutVersionPath = *VersionJsonPath;

	OutAllAsset2PakIDFilePath = FString::Printf(TEXT("%s/%s/%s/AllAsset2PakID.json"), *BuildDataPath, *strPlatform, FApp::GetProjectName());
	OutTypeResourceFilePath = FString::Printf(TEXT("%s/%s/%s/m_TypeResource.json"), *BuildDataPath, *strPlatform, FApp::GetProjectName());
	OutTypePakNumFilePath = FString::Printf(TEXT("%s/%s/%s/m_TypePakNum.json"), *BuildDataPath, *strPlatform, FApp::GetProjectName());
	OutTempFileNoSpecifyPath = FString::Printf(TEXT("%s/%s/%s/m_TempFileNoSpecify2Pak.json"), *BuildDataPath, *strPlatform, FApp::GetProjectName());

	FString PakListsDir = FString::Printf(TEXT("%s/%s/%s/PakLists/"), *BuildDataPath, *m_strPlatform, FApp::GetProjectName());
	FString PaksDir = FString::Printf(TEXT("%s/%s/%s/Content/Paks/"), *BuildDataPath, *m_strPlatform, FApp::GetProjectName());

	if (!FPaths::FileExists(*VersionJsonPath))
	{
		OutCheckResult.Append(FString::Printf(TEXT("\t%s is not exist!!\n"), *VersionJsonPath));
		bHasVersionJson = false;
	}
	
	if (!FPaths::FileExists(*CryptoName))
	{
		OutCheckResult.Append(FString::Printf(TEXT("\t%s is not exist!!\n"), *CryptoName));
		bHasCrypto = false;
	}

	//if (!FPaths::FileExists(*CookerOpenOrderName))
	//{
	//	OutCheckResult.Append(FString::Printf(TEXT("\t%s is not exist!!\n"), *CookerOpenOrderName));
	//	bHasCookerOpenOrder = false;
	//}

	if (!FPaths::FileExists(*OutAllAsset2PakIDFilePath))
	{
		OutCheckResult.Append(FString::Printf(TEXT("\t%s is not exist!!\n"), *OutAllAsset2PakIDFilePath));
		bHasAllAsset2PakIDFile = false;
	}

	if (!FPaths::FileExists(*OutTypeResourceFilePath))
	{
		OutCheckResult.Append(FString::Printf(TEXT("\t%s is not Exists!!!"), *OutTypeResourceFilePath));
		bHasTypeResourceFile = false;
		return false;
	}

	if (!FPaths::FileExists(*OutTypePakNumFilePath))
	{
		OutCheckResult.Append(FString::Printf(TEXT("\t%s is not Exists!!!"), *OutTypePakNumFilePath));
		bHasTypePakNumFile = false;
		return false;
	}

	//判断文件夹是否存在
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*PakListsDir))
	{
		OutCheckResult.Append(FString::Printf(TEXT("\t%s Directory is not Exists!!!"), *PakListsDir));
		bHasDirPakLists = false;
		return false;
	}

	if (!PlatformFile.DirectoryExists(*PaksDir))
	{
		OutCheckResult.Append(FString::Printf(TEXT("\t%s Directory is not Exists!!!"), *PaksDir));
		bHasDirPaks = false;
		return false;
	}
	
	return bHasCrypto && bHasCookerOpenOrder && bHasVersionJson && bHasAllAsset2PakIDFile && bHasTypeResourceFile && bHasTypePakNumFile && bHasDirPakLists && bHasDirPaks && bHasTempFileNoSpecifyFile;
}

void SHotPatcherCreatePaks::CopyCryptoFile(FString& InCryptoPath)
{
	FString strPlatform = m_strPlatform;
	FString moveCryptoPath = FString::Printf(TEXT("%sBuildData/%s/%s/Crypto.json"), *FPaths::ProjectDir(), *strPlatform, FApp::GetProjectName());

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (PlatformFile.CopyFile(*moveCryptoPath, *InCryptoPath))
	{
		UE_LOG(LogTemp, Log, TEXT("=======================BackUpFileToBuildData: %s."), *moveCryptoPath);
	}
}

bool SHotPatcherCreatePaks::DealAddAssetPak(FPatchVersionDiff& VersionDiffInfo, const ETargetPlatform& Platform, FChunkInfo& Chunk, TArray<FPakCommand>& AdditionalFileToPak, FString& AllAsset2PakIDFilePath,FString& TypeResourceFilePath,FString& TypePakNumFilePath,FString& TempFileNoSpecifyPath)
{
	//清空ModifyExternalFiles
	TArray<FExternFileInfo> ModifyExternalFiles;
	ModifyExternalFiles = VersionDiffInfo.PlatformExternDiffInfo.Find(Platform)->ModifyExternalFiles;
	VersionDiffInfo.PlatformExternDiffInfo.Find(Platform)->ModifyExternalFiles.Empty();

	TArray<FPakCommand> ChunkPakCommands = UFlibPatchParserHelper::CollectPakCommandByChunk(VersionDiffInfo, Chunk, m_strPlatform, GetSettingObject()->GetPakCommandOptions());

	ChunkPakCommands.Append(AdditionalFileToPak);

	TArray<FReplaceText> ReplacePakCommandTexts = GetSettingObject()->GetReplacePakCommandTexts();
	TArray<FString> PakCommands = UFlibPatchParserHelper::GetPakCommandStrByCommands(ChunkPakCommands, ReplacePakCommandTexts);

	//保存整包命令文件(缺少额外文件的修改部分:ModifyExternalFiles)
	FString PakCommandSavePath = FString::Printf(TEXT("%s/%s/%s/PakLists/%s_%s_All_PakCommands.txt"), 
		*m_CreatePaksSettings->GetBuildDataPath(),
		*m_strPlatform, FApp::GetProjectName(),
		*m_CreatePaksSettings->VersionId,
		*m_strPlatform);

	bool PakCommandSaveStatus = FFileHelper::SaveStringArrayToFile(
		PakCommands,
		*PakCommandSavePath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

	//加载并保存之前版本的AllAsset2PakID.json文件
	LoadAllAsset2PakIDJsonFile(AllAsset2PakIDFilePath);

	//设置增量包内容
	for (auto itemPakFile : PakCommands)
	{
		auto index = itemPakFile.Find("\" \"../../../");
		auto path = itemPakFile.Mid(1, index - 1);
		FPaths::NormalizeFilename(path);
		FPaths::NormalizeFilename(itemPakFile);
		UE_LOG(LogTemp, Log, TEXT("=======================m_AllPakFileList NormalizeFilename: %s. %s"), *path, *itemPakFile);
		m_AllPakFileCommandsFromPakListLogs.Add(path, itemPakFile);
	}

	TArray<FAssetDetail> AllAssets;
	UFLibAssetManageHelperEx::GetAssetDetailsByAssetDependenciesInfo(VersionDiffInfo.AssetDiffInfo.AddAssetDependInfo, AllAssets);
	
	//加载之前的m_TypeResource.json文件
	LoadTypeResourceFile(TypeResourceFilePath);

	//加载之前的m_TypePakNum.json文件
	LoadTypePakNumFile(TypePakNumFilePath);

	//加载之前的m_TempFileNoSpecify2Pak.json文件
	LoadTempFileNoSpecify2PakFile(TempFileNoSpecifyPath);

	//补充m_TypeResource内容
	for (const auto& Item : AllAssets)
	{
		if (Item.mPackagePath.StartsWith("/Engine/", ESearchCase::IgnoreCase))
		{
			continue;
		}

		if (!m_TypeResource.Contains(Item.mAssetType))
		{
			TArray<FString> assetsList = TArray<FString>{ Item.mPackagePath };
			m_TypeResource.Add(Item.mAssetType, assetsList);
			UE_LOG(LogTemp, Log, TEXT("=======================Item.mAssetType is %s %s."), *Item.mAssetType, *Item.mPackagePath);
		}
		else
		{
			m_TypeResource[Item.mAssetType].AddUnique(Item.mPackagePath);
		}

	}

	//添加EngineAndTemp类型管理
	FString EngineAndTempAssetType = "EngineAndTemp";
	if (!m_TypePakNum.Contains(EngineAndTempAssetType))
	{
		INT pakNum = 0;
		TArray<INT> temp;
		temp.Add(pakNum);
		m_TypePakNum.Add(EngineAndTempAssetType, temp);
	}
	FString EngineAndTempPatchAssetType = "EngineAndTemp_P";
	if (!m_TypePakNum.Contains(EngineAndTempPatchAssetType))
	{
		INT pakNum = 0;
		TArray<INT> temp;
		temp.Add(pakNum);
		m_TypePakNum.Add(EngineAndTempPatchAssetType, temp);
	}

	//增量包需要增加lastpakNum
	for (auto& ItemTypePakNum : m_TypePakNum)
	{
		INT lastpakNum = 0;
		lastpakNum = ItemTypePakNum.Value.Last() + 1;
		ItemTypePakNum.Value.Add(lastpakNum);
	}

	//补充m_TypePakNum内容
	for (const auto& Item : AllAssets)
	{
		//m_File2FAssetDetail m_TypePakNum 必须是全部资源，否则会找不到对应的资源类型
		m_File2FAssetDetail.Add(Item.mPackagePath, Item);
		if (!m_TypePakNum.Contains(Item.mAssetType))
		{
			INT pakNum = 0;
			TArray<INT> temp;
			temp.Add(pakNum);
			m_TypePakNum.Add(Item.mAssetType, temp);
		}
	}
	for (auto type : m_TypeResource)
	{
		if (!m_TypePakNum.Contains(type.Key))
		{
			INT pakNum = 0;
			TArray<INT> temp;
			temp.Add(pakNum);
			m_TypePakNum.Add(type.Key, temp);
		}
	}

	FillTypeCountSize();

	FString strReourceName;
	INT lastpakNum = 0;
	FString PakName = "";

	INT lastpakNum_P = m_TypePakNum[EngineAndTempPatchAssetType].Last();

	for (const auto& Item : AllAssets)
	{
		strReourceName = Item.mPackagePath;
		lastpakNum = m_TypePakNum[m_File2FAssetDetail[strReourceName].mAssetType].Last();
		PakName = FString::Printf(TEXT("%s_%s_%d_DLC"), *m_strPlatform, *m_File2FAssetDetail[strReourceName].mAssetType, lastpakNum);

		if (isPakFull(strReourceName, PakName, m_File2FAssetDetail[strReourceName].mAssetType))
		{
			lastpakNum = lastpakNum + 1;
			PakName = FString::Printf(TEXT("%s_%s_%d_DLC"), *m_strPlatform, *m_File2FAssetDetail[strReourceName].mAssetType, lastpakNum);
			m_TypePakNum[m_File2FAssetDetail[strReourceName].mAssetType].Add(lastpakNum);
		}

		//TODO:这里直接用P包做增量，使未关联的重复文件，在增量更新时也具有优先级,后续可以专门做判断去优化
		FString PakName_EngineAndTemp = FString::Printf(TEXT("%s_EngineAndTemp_DLC_%s_P"), *m_strPlatform,*m_CreatePaksSettings->VersionId);

		FString CheckPakName;
		if (!m_AllAsset2PakID_JsonObj->TryGetStringField(strReourceName, CheckPakName))
		{
			if (strReourceName.Contains("/Engine/") || strReourceName.Contains("/Game/Config/") || strReourceName.Contains("/Game/Plugins/") ||
				strReourceName.Contains("\\Engine\\") || strReourceName.Contains("\\Game\\Config\\") || strReourceName.Contains("\\Game\\Plugins\\"))
			{
				PakName = PakName_EngineAndTemp;
			}

			m_AllAsset2PakID_JsonObj->SetStringField(strReourceName, PakName);

			if (m_PakID2FileList.Contains(PakName))
			{
				m_PakID2FileList[PakName].AddUnique(strReourceName);
			}
			else
			{
				TArray<FString> TempArray;
				TempArray.AddUnique(strReourceName);
				m_PakID2FileList.Add(PakName, TempArray);
			}
		}
		else
		{
			//若原本完整包内已有包含，则打进新的P包内(防止重复打包同名文件，具有优先级)
			PakName = FString::Printf(TEXT("%s_%d_P"), *CheckPakName, lastpakNum_P);
			m_AllAsset2PakID_JsonObj->SetStringField(strReourceName, PakName);

			if (m_PakID2FileList.Contains(PakName))
			{
				m_PakID2FileList[PakName].AddUnique(strReourceName);
			}
			else
			{
				TArray<FString> TempArray;
				TempArray.AddUnique(strReourceName);
				m_PakID2FileList.Add(PakName, TempArray);
			}

			UE_LOG(LogTemp, Error, TEXT("=======================SplitPaks strReourceName is Already In Pak!!,Please Check it!!! strReourceName : %s , CheckPakName : %s"), *strReourceName, *CheckPakName);
		}

	}
	//增加Temp文件管理
	FillTempFilesinPak(true);

	FillExternFilesInPak(true);

	FillSceneDic_add();

	//生成m_PakID2AllPakFileCommondsDic
	FillPakID2AllPakFileCommondsDic_Add();

	TArray<FString> PakNameList;
	for (const auto& PakItem : m_PakID2AllPakFileCommondsDic)
	{
		PakNameList.AddUnique(PakItem.Key);
	}

	//处理ModifyExternalFiles部分
	for (const auto& modify_file : ModifyExternalFiles)
	{
		if (m_TempFileNoSpecify2Pak.Contains(modify_file.FilePath.FilePath))
		{
			PakNameList.AddUnique(m_TempFileNoSpecify2Pak[modify_file.FilePath.FilePath]);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("=======================SplitPaks modify_file not Exist!!!,Please Check it!!! modify_file.FilePath : %s"), *modify_file.FilePath.FilePath);
		}
	}

	if (!ExecuteUnrealPakProcess_ByDiff(PakNameList,true))
	{
		return false;
	}

	return true;
}

void SHotPatcherCreatePaks::WriteTypeResourceToJson(FString FileSaveName,bool is_add)
{
	TSharedPtr<FJsonObject> RootJsonObject = MakeShareable(new FJsonObject);

	for (const auto& ItemTypeResource : m_TypeResource)
	{
		TArray<FString> InArray;
		for (const auto& mPackagePath : ItemTypeResource.Value)
		{
			InArray.AddUnique(mPackagePath);
		}

		TArray<TSharedPtr<FJsonValue>> ArrayJsonValueList;
		for (const auto& ArrayItem : InArray)
		{
			ArrayJsonValueList.Add(MakeShareable(new FJsonValueString(ArrayItem)));
		}

		RootJsonObject->SetArrayField(ItemTypeResource.Key, ArrayJsonValueList);
	}

	FString OutString;
	auto JsonWriter = TJsonWriterFactory<TCHAR>::Create(&OutString);
	FJsonSerializer::Serialize(RootJsonObject.ToSharedRef(), JsonWriter);
	FString RecursiveDictToJson = FString::Printf(TEXT("%sBuildData/%s/%s/%s"), *FPaths::ProjectDir(), *m_strPlatform, FApp::GetProjectName(), *FileSaveName);
	if (is_add)
	{
		FString BuildDataPath = m_CreatePaksSettings->GetBuildDataPath();
		RecursiveDictToJson = FString::Printf(TEXT("%s/%s/%s/%s"), *BuildDataPath, *m_strPlatform, FApp::GetProjectName(), *FileSaveName);
	}
	if (UFLibAssetManageHelperEx::SaveStringToFile(RecursiveDictToJson, OutString))
	{
		auto Msg = LOCTEXT("RecursiveDictToJson", "Succeed to export Type Resource info.");
		UFlibHotPatcherEditorHelper::CreateSaveFileNotify(Msg, RecursiveDictToJson);
	}
}

void SHotPatcherCreatePaks::WriteTypePakNumToJson(FString FileSaveName,bool is_add)
{
	TSharedPtr<FJsonObject> RootJsonObject = MakeShareable(new FJsonObject);

	for (const auto& ItemTypePakNum : m_TypePakNum)
	{
		TArray<INT> InArray;
		for (const auto& mPackagePath : ItemTypePakNum.Value)
		{
			InArray.AddUnique(mPackagePath);
		}

		TArray<TSharedPtr<FJsonValue>> ArrayJsonValueList;
		for (const auto& ArrayItem : InArray)
		{
			ArrayJsonValueList.Add(MakeShareable(new FJsonValueNumber(ArrayItem)));
		}

		RootJsonObject->SetArrayField(ItemTypePakNum.Key, ArrayJsonValueList);
	}

	FString OutString;
	auto JsonWriter = TJsonWriterFactory<TCHAR>::Create(&OutString);
	FJsonSerializer::Serialize(RootJsonObject.ToSharedRef(), JsonWriter);
	FString RecursiveDictToJson = FString::Printf(TEXT("%sBuildData/%s/%s/%s"), *FPaths::ProjectDir(), *m_strPlatform, FApp::GetProjectName(), *FileSaveName);
	if (is_add)
	{
		FString BuildDataPath = m_CreatePaksSettings->GetBuildDataPath();
		RecursiveDictToJson = FString::Printf(TEXT("%s/%s/%s/%s"), *BuildDataPath, *m_strPlatform, FApp::GetProjectName(), *FileSaveName);
	}
	if (UFLibAssetManageHelperEx::SaveStringToFile(RecursiveDictToJson, OutString))
	{
		auto Msg = LOCTEXT("RecursiveDictToJson", "Succeed to export Type PakNum info.");
		UFlibHotPatcherEditorHelper::CreateSaveFileNotify(Msg, RecursiveDictToJson);
	}
}

void SHotPatcherCreatePaks::LoadTypeResourceFile(FString& InFilePath)
{
	FString FileContent;

	if (UFLibAssetManageHelperEx::LoadFileToString(InFilePath, FileContent))
	{
		TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(FileContent);
		TSharedPtr<FJsonObject> DeserializeJsonObject;
		FJsonSerializer::Deserialize(JsonReader, DeserializeJsonObject);
		
		if (!!DeserializeJsonObject->Values.Num())
		{
			for (const auto& JsonObjItem:DeserializeJsonObject->Values)
			{
				TArray<FString> assetsList;
				if (DeserializeJsonObject->TryGetStringArrayField(JsonObjItem.Key, assetsList))
				{
					if (!m_TypeResource.Contains(JsonObjItem.Key))
					{
						m_TypeResource.Add(JsonObjItem.Key, assetsList);
					}
					else
					{
						m_TypeResource[JsonObjItem.Key].Append(assetsList);
					}
				}
			}
		}
	}
}

void SHotPatcherCreatePaks::LoadTypePakNumFile(FString& InFilePath)
{
	FString FileContent;

	if (UFLibAssetManageHelperEx::LoadFileToString(InFilePath, FileContent))
	{
		TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(FileContent);
		TSharedPtr<FJsonObject> DeserializeJsonObject;
		FJsonSerializer::Deserialize(JsonReader, DeserializeJsonObject);

		if (!!DeserializeJsonObject->Values.Num())
		{
			for (const auto& JsonObjItem : DeserializeJsonObject->Values)
			{
				const TArray<TSharedPtr<FJsonValue>>* ArrayJsonValueList;
				if (DeserializeJsonObject->TryGetArrayField(JsonObjItem.Key, ArrayJsonValueList))
				{
					TArray<INT> InArray;
					for (const auto& ValueItem : *ArrayJsonValueList)
					{
						InArray.Add(ValueItem->AsNumber());
					}

					if (!m_TypePakNum.Contains(JsonObjItem.Key))
					{
						m_TypePakNum.Add(JsonObjItem.Key, InArray);
					}
					else
					{
						m_TypePakNum[JsonObjItem.Key].Append(InArray);
					}
				}
			}
		}
	}
}

void SHotPatcherCreatePaks::FillPakID2AllPakFileCommondsDic_Add()
{
	FString ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	TArray<FString> CookedAssetAbsPath;
	TArray<FString> CookedAssetRelativePath;
	TArray<FString> FinalCookedCommand;
	TArray<FString> InCookParams;

	InCookParams.Add("-compress");

	for (const auto& ItemPakID2FileList : m_PakID2FileList)
	{
		for (auto& InAssetLongPackageName : ItemPakID2FileList.Value)
		{
			CookedAssetAbsPath.Empty();
			CookedAssetRelativePath.Empty();
			FinalCookedCommand.Empty();

			FString AssetCookedRelativePath = InAssetLongPackageName;

			// remove .uasset / .umap postfix
			{
				int32 lastDotIndex = 0;
				AssetCookedRelativePath.FindLastChar('.', lastDotIndex);
				AssetCookedRelativePath.RemoveAt(lastDotIndex, AssetCookedRelativePath.Len() - lastDotIndex);
			}

			if (UFLibAssetManageHelperEx::ConvLongPackageNameToCookedPath(ProjectDir, m_strPlatform, AssetCookedRelativePath, CookedAssetAbsPath, CookedAssetRelativePath))
			{
				UFLibAssetManageHelperEx::CombineCookedAssetCommand(CookedAssetAbsPath, CookedAssetRelativePath, InCookParams, FinalCookedCommand);

				for (auto itemCookedAssetAbsPath : CookedAssetAbsPath)
				{
					TMap<FString, FString> tempDic;
					if (m_AllPakFileCommandsFromPakListLogs.Contains(itemCookedAssetAbsPath))
					{
						if (!m_PakID2AllPakFileCommondsDic.Contains(ItemPakID2FileList.Key))
						{
							tempDic.Empty();
							tempDic.Add(itemCookedAssetAbsPath, m_AllPakFileCommandsFromPakListLogs[itemCookedAssetAbsPath]);
							m_PakID2AllPakFileCommondsDic.Add(ItemPakID2FileList.Key, tempDic);
						}
						else
						{
							if (!(m_PakID2AllPakFileCommondsDic[ItemPakID2FileList.Key]).Contains(itemCookedAssetAbsPath))
							{
								(m_PakID2AllPakFileCommondsDic[ItemPakID2FileList.Key]).Add(itemCookedAssetAbsPath, m_AllPakFileCommandsFromPakListLogs[itemCookedAssetAbsPath]);
							}
						}
					}
					else
					{
						UE_LOG(LogTemp, Log, TEXT("=======================SplitPaks NOT NOT NOT NOT in PAK : %s."), *itemCookedAssetAbsPath);
						//for (auto icmd : FinalCookedCommand)
						//{
						//	if (icmd.Contains(itemCookedAssetAbsPath))
						//	{
						//		if (!m_PakID2AllFileList.Contains(ItemPakID2FileList.Key))
						//		{
						//			tempDic.Empty();
						//			tempDic.Add(itemCookedAssetAbsPath, icmd);
						//			m_PakID2AllFileList.Add(ItemPakID2FileList.Key, tempDic);
						//		}
						//		else
						//		{
						//			if (!(m_PakID2AllFileList[ItemPakID2FileList.Key]).Contains(itemCookedAssetAbsPath))
						//			{
						//				(m_PakID2AllFileList[ItemPakID2FileList.Key]).Add(itemCookedAssetAbsPath, icmd);
						//				UE_LOG(LogTemp, Log, TEXT("=======================SplitPaks icmdicmdicmdicmdicmdicmd: %s."), *icmd);
						//			}
						//		}
						//	}
						// }
					}
				}
			}
		}
	}

	TArray<FString> FileListInPAK;
	for (auto itemPakID2AllFileDic : m_PakID2AllPakFileCommondsDic)
	{
		for (auto item : itemPakID2AllFileDic.Value)
		{
			FileListInPAK.Add(item.Key);
		}
	}

	TArray<FString> RelativePathList;
	m_TempFileRelativePath.GetKeys(RelativePathList);

	//EngineAndTemp
	FString EngineAndTempAssetType = "EngineAndTemp";
	INT lastpakNum = m_TypePakNum[EngineAndTempAssetType].Last();
	//lastpakNum = lastpakNum + 1;
	//m_TypePakNum[EngineAndTempAssetType].Add(lastpakNum);

	for (auto itemFile : m_AllPakFileCommandsFromPakListLogs)
	{
		if (!FileListInPAK.Contains(itemFile.Key))
		{
			FString PakName = FString::Printf(TEXT("%s_%s_%d_DLC"), *m_strPlatform, *EngineAndTempAssetType, lastpakNum);
			//FString PakName = FString::Printf(TEXT("%s_EngineAndTemp_0_Apk"), *m_strPlatform);
			
			//添加额外文件分包判断
			FString RelativePath;
			FString DirectoryPath;
			if (TempFile2PakIsContains(itemFile.Value, RelativePathList, RelativePath))
			{
				if (m_TempFileRelativePath.Contains(RelativePath))
				{
					PakName = m_TempFileRelativePath[RelativePath];
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("=======================SplitPaks EngineAndTemp : m_TempFileRelativePath cant find RelativePath!!! RelativePath = %s"), *RelativePath);
					return;
				}
			}
			else if (m_TempFile2Pak.Contains(itemFile.Key))
			{
				PakName = m_TempFile2Pak[itemFile.Key];
			}
			else if (m_ExternFiles2Pak.Contains(itemFile.Key))
			{
				PakName = m_ExternFiles2Pak[itemFile.Key];
				m_TempFileNoSpecify2Pak.Add(itemFile.Key, PakName);
			}
			else if (IsInExternDir(itemFile.Key, DirectoryPath))
			{
				PakName = m_ExternDirectory2Pak[DirectoryPath];
				m_TempFileNoSpecify2Pak.Add(itemFile.Key, PakName);
			}
			else
			{
				//非指定包文件，添加索引
				m_TempFileNoSpecify2Pak.Add(itemFile.Key, PakName);
			}

			FileListInPAK.Add(itemFile.Key);
			TMap<FString, FString> tempDic;
			if (!m_PakID2AllPakFileCommondsDic.Contains(PakName))
			{
				tempDic.Empty();
				tempDic.Add(itemFile.Key, itemFile.Value);
				m_PakID2AllPakFileCommondsDic.Add(PakName, tempDic);
			}
			else
			{
				if (!(m_PakID2AllPakFileCommondsDic[PakName]).Contains(itemFile.Key))
				{
					(m_PakID2AllPakFileCommondsDic[PakName]).Add(itemFile.Key, itemFile.Value);
				}
			}
		}
	}

	//FString PakListsDir = FString::Printf(TEXT("%sBuildData/%s/%s/PakLists/"), *FPaths::ProjectDir(), *m_strPlatform, FApp::GetProjectName());
	//CreateDir(PakListsDir);

	FString BuildDataPath = m_CreatePaksSettings->GetBuildDataPath();

	for (auto itemPakID2AllFileDic : m_PakID2AllPakFileCommondsDic)
	{
		FString SavePath = FString::Printf(TEXT("%s/%s/%s/PakLists/%s.txt"), *BuildDataPath, *m_strPlatform, FApp::GetProjectName(), *itemPakID2AllFileDic.Key);

		TArray<FString> OutPakCommand;

		//生成包文件已存在，则往后更新追加
		if (FPaths::FileExists(SavePath))
		{
			FFileHelper::LoadFileToStringArray(OutPakCommand, *SavePath);
		}

		for (auto item : itemPakID2AllFileDic.Value)
		{
			OutPakCommand.Add(item.Value);
		}
		FFileHelper::SaveStringArrayToFile(OutPakCommand, *SavePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	if (FileListInPAK.Num() != m_AllPakFileCommandsFromPakListLogs.Num())
	{
		UE_LOG(LogTemp, Error, TEXT("=======================SplitPaks CookedPath : FileListInPAK.Num():%d m_AllPakFileCommandsFromPakListLogs.Num() %d "), FileListInPAK.Num(), m_AllPakFileCommandsFromPakListLogs.Num());
	}

}

void SHotPatcherCreatePaks::WriteTempFile2PakToJson(FString FileSaveName)
{
	TSharedPtr<FJsonObject> RootJsonObject = MakeShareable(new FJsonObject);

	for (const auto& no_assets2Pak : m_TempFile2Pak)
	{
		RootJsonObject->SetStringField(no_assets2Pak.Key, no_assets2Pak.Value);
	}

	FString OutString;
	auto JsonWriter = TJsonWriterFactory<TCHAR>::Create(&OutString);
	FJsonSerializer::Serialize(RootJsonObject.ToSharedRef(), JsonWriter);
	FString File2PakToJson = FString::Printf(TEXT("%sBuildData/%s/%s/%s"), *FPaths::ProjectDir(), *m_strPlatform, FApp::GetProjectName(), *FileSaveName);
	if (UFLibAssetManageHelperEx::SaveStringToFile(File2PakToJson, OutString))
	{
		auto Msg = LOCTEXT("NoAssetFile2PakToJson", "Succeed to export Asset Related info.");
		UFlibHotPatcherEditorHelper::CreateSaveFileNotify(Msg, File2PakToJson);
	}
}

void SHotPatcherCreatePaks::WriteTempFileNoSpecify2PakToJson(FString FileSaveName,bool is_add)
{
	TSharedPtr<FJsonObject> RootJsonObject = MakeShareable(new FJsonObject);

	for (const auto& no_assets2Pak : m_TempFileNoSpecify2Pak)
	{
		RootJsonObject->SetStringField(no_assets2Pak.Key, no_assets2Pak.Value);
	}

	FString OutString;
	auto JsonWriter = TJsonWriterFactory<TCHAR>::Create(&OutString);
	FJsonSerializer::Serialize(RootJsonObject.ToSharedRef(), JsonWriter);
	FString File2PakToJson = FString::Printf(TEXT("%sBuildData/%s/%s/%s"), *FPaths::ProjectDir(), *m_strPlatform, FApp::GetProjectName(), *FileSaveName);
	if (is_add)
	{
		File2PakToJson = FString::Printf(TEXT("%s/%s/%s/%s"), *m_CreatePaksSettings->GetBuildDataPath(), *m_strPlatform, FApp::GetProjectName(), *FileSaveName);
	}
	if (UFLibAssetManageHelperEx::SaveStringToFile(File2PakToJson, OutString))
	{
		auto Msg = LOCTEXT("NoAssetFile2PakToJson", "Succeed to export Asset Related info.");
		UFlibHotPatcherEditorHelper::CreateSaveFileNotify(Msg, File2PakToJson);
	}
}

void SHotPatcherCreatePaks::GetCookedIniRelativePath(const FString& IniFile,FString& OutCookedIniRelativePath)
{
	OutCookedIniRelativePath.Empty();

	FString InProjectAbsDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	FString InProjectName = UFlibPatchParserHelper::GetProjectName();
	FString InEngineAbsDir = FPaths::ConvertRelativePathToFull(FPaths::EngineDir());

	bool bIsInProjectIni = false;
	if (IniFile.Contains(InProjectAbsDir))
	{
		bIsInProjectIni = true;
	}

	FString IniAbsDir;
	FString IniFileName;
	FString IniExtention;
	FPaths::Split(IniFile, IniAbsDir, IniFileName, IniExtention);

	FString RelativePath;

	if (bIsInProjectIni)
	{
		RelativePath = FString::Printf(
			TEXT("../../../%s/"),
			*InProjectName
		);

		FString RelativeToProjectDir = UKismetStringLibrary::GetSubstring(IniAbsDir, InProjectAbsDir.Len(), IniAbsDir.Len() - InProjectAbsDir.Len());
		RelativePath = FPaths::Combine(RelativePath, RelativeToProjectDir);
	}
	else
	{
		RelativePath = FString::Printf(
			TEXT("../../../Engine/")
		);
		FString RelativeToEngineDir = UKismetStringLibrary::GetSubstring(IniAbsDir, InEngineAbsDir.Len(), IniAbsDir.Len() - InEngineAbsDir.Len());
		RelativePath = FPaths::Combine(RelativePath, RelativeToEngineDir);
	}

	FString IniFileNameWithExten = FString::Printf(TEXT("%s.%s"), *IniFileName, *IniExtention);
	OutCookedIniRelativePath = FPaths::Combine(RelativePath, IniFileNameWithExten);
}

bool SHotPatcherCreatePaks::TempFile2PakIsContains(const FString& IniFile, TArray<FString>& RelativePathList , FString& OutRelativePath)
{
	OutRelativePath.Empty();

	bool isContains = false;

	auto index = IniFile.Find("\" \"../../../");
	FString PakListLogsCookedIniRelativePath = IniFile.Mid(index + 3, IniFile.Len() - index - 4);
	//GetCookedIniRelativePath(IniFile.Key, PakListLogsCookedIniRelativePath);

	if (RelativePathList.Contains(PakListLogsCookedIniRelativePath))
	{
		isContains = true;
		OutRelativePath = PakListLogsCookedIniRelativePath;
	}

	return isContains;
}

#undef LOCTEXT_NAMESPACE