///////////////////////////////////////////////////////////////////////////

// #include "HotPatcherPrivatePCH.h"
#include "CreatePatch/CreatePaks.h"

#include "FlibHotPatcherEditorHelper.h"
#include "FLibAssetManageHelperEx.h"
#include "AssetManager/FAssetDependenciesInfo.h"
#include "FHotPatcherVersion.h"


// engine header
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Layout/SSeparator.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopedSlowTask.h"

#include "FPakFileInfo.h"
#include "ThreadUtils/FThreadUtils.hpp"
#include "PakFileUtilities.h"

/// <summary>
/// 解析FDLCFeatureConfig
/// </summary>
/// <returns></returns>

TArray<FDLCFeatureConfig> UCreatePaks::DeSerializeFDLCFeatureConfig(const FString& InJsonContent)
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


bool UCreatePaks::FindAllRefFiles(FString AssetPath, TMap<FString, FString>& RefDic)
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


bool UCreatePaks::WriteScene2PaksDictToJson(FString FileSaveName)
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
	FString RecursiveDictToJson = FString::Printf(TEXT("%sBuildData/%s/%s/%s"), *FPaths::ProjectDir(), *m_strPlatform, FApp::GetProjectName(), *FileSaveName);
	return true;
}

bool UCreatePaks::WriteFile2PakDictToJson(FString FileSaveName)
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
	return true;
}

bool UCreatePaks::WriteRecursiveDictToJson(FString FileSaveName)
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

	return true;
}


void UCreatePaks::ScanDirectoryRecursive(TArray<FString>& Files, const FString& FilePath, const FString& Extension)
{
	IFileManager::Get().FindFilesRecursive(Files, *FilePath, *Extension, true, false);
}

// 遍历文件夹下指定类型文件
// Files 保存遍例到的所有文件
// FilePath 文件夹路径  如 "D:\\MyCodes\\LearnUE4Cpp\\Source\\LearnUE4Cpp\\"
// Extension 扩展名(文件类型) 如 "*.cpp"
void UCreatePaks::ScanDirectory(TArray<FString>& Files, const FString& FilePath, const FString& Extension)
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

bool UCreatePaks::GetPakCMDs()
{
	bool bResult = false;
	return bResult;
}
/// <summary>
/// 该类型资源不进dlc，比如
/// </summary>
/// <param name="ResourceType"></param>
/// <returns></returns>
bool UCreatePaks::GetResourceTypeMustInApk(FString& ResourceType)
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

bool UCreatePaks::GetPakOMandatoryFilesByResourceType()
{
	bool bResult = false;

	//	TArray<FAssetDetail> m_AllPak0Assets;
	//TMap<FString, FString> m_AllAsset2PakID;
	//TMap<FString, TArray<FString>> m_PakID2FileList;
	//TMap<FString, TArray<FString>> m_Scene2FileList;
	TArray<FString> InFilterPackagePaths = TArray<FString>{ "/Game/UI", "/Game/Effects", "/Game/Characters" };
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
void UCreatePaks::FillTypeCountSize()
{
	TMap<INT, INT> tempdic;
	tempdic.Add(50, 1024 * 10);
	m_TypeCountSize.Add("Default", tempdic);
	tempdic.Empty();
	tempdic.Add(30, 1024 * 10);
	m_TypeCountSize.Add("Font", tempdic);
	tempdic.Empty();
	tempdic.Add(1, 1024 * 10);
	m_TypeCountSize.Add("PrimaryAssetLabel", tempdic);
	tempdic.Empty();
	tempdic.Add(1, 1024 * 10);
	m_TypeCountSize.Add("World", tempdic);
	tempdic.Empty();
	tempdic.Add(20, 1024 * 10);
	m_TypeCountSize.Add("SkeletalMesh", tempdic);
	tempdic.Empty();
	tempdic.Add(30, 1024 * 10);
	m_TypeCountSize.Add("StaticMesh", tempdic);
	tempdic.Empty();
	tempdic.Add(20, 1024 * 10);
	m_TypeCountSize.Add("ParticleSystem", tempdic);
	tempdic.Empty();
	tempdic.Add(30, 1024 * 10);
	m_TypeCountSize.Add("Texture2D", tempdic);
	tempdic.Empty();
	tempdic.Add(20, 1024 * 10);
	m_TypeCountSize.Add("Material", tempdic);
	tempdic.Empty();
	tempdic.Add(20, 1024 * 10);
	m_TypeCountSize.Add("MaterialFunction", tempdic);
	tempdic.Empty();
	tempdic.Add(20, 1024 * 10);
	m_TypeCountSize.Add("PhysicalMaterial", tempdic);
	tempdic.Empty();
	tempdic.Add(20, 1024 * 10);
	m_TypeCountSize.Add("MaterialInstanceConstant", tempdic);
	tempdic.Empty();
	tempdic.Add(20, 1024 * 10);
	m_TypeCountSize.Add("BlendSpace", tempdic);
	tempdic.Empty();
	tempdic.Add(30, 1024 * 10);
	m_TypeCountSize.Add("SoundCue", tempdic);
	tempdic.Empty();
	tempdic.Add(20, 1024 * 10);
	m_TypeCountSize.Add("SoundWave", tempdic);
	tempdic.Empty();
	tempdic.Add(20, 1024 * 10);
	m_TypeCountSize.Add("SoundAttenuation", tempdic);
	tempdic.Empty();
	tempdic.Add(20, 1024 * 10);
	m_TypeCountSize.Add("SoundClass", tempdic);
	//tempdic.Empty();
	//tempdic.Add(20, 1024 * 10);
	//m_TypeCountSize.Add("SoundConcurrency", tempdic);
}


//	TMap<FString, TArray<FString>> m_TypeResource;
//  TMap<FString, FString> m_apkMapDic;
//  TMap<FString, FString> m_dlcMapDic;
void UCreatePaks::FillTypeResourceSilptDLCandAPKLevel()
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
			m_TypeResource[Item.mAssetType].Add(Item.mPackagePath);
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


bool UCreatePaks::isPakFull(FString& AssetPath, FString& PakName, FString& resrouceType)
{
	bool bResult = false;
	bool bCount = false;
	//bool bSize = false;

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
	}

	//todo add size control

	bResult = bCount;

	return bResult;
}


// TMap<FString, TArray<FString>> m_File2DirectDependence;
// TMap<FString, TArray<FString>> m_File2RecursionDependence;
void UCreatePaks::FillDirectAndRecursionDependence()
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

	m_File2RecursionDependence.KeySort([](FString A, FString B) {return A > B;});

	WriteRecursiveDictToJson("RecursiveDictToJson.json");
}

bool UCreatePaks::AssetResoruceFillinPak(FString& AssetResoruce, FString& PakName, FString& SceneName)
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
			PakName =  PakName_EngineAndTemp;
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

bool UCreatePaks::FillSceneDic()
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

bool UCreatePaks::AssetResoruceFillinScene(FString& AssetResoruce, FString& PakName, FString& SceneName)
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


bool UCreatePaks::SpiltPakByResourceType()
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

	//GetPakOMandatoryFiles();先处理ui effect等
	//GetPakOMandatoryFilesByResourceType();

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

	FillSceneDic();

	FillPakID2AllPakFileCommondsDic();

	ExecuteUnrealPakProcess();

	return bResult;
}

bool UCreatePaks::FillPakID2AllPakFileCommondsDic()
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
	//EngineAndTemp
	for (auto itemFile : m_AllPakFileCommandsFromPakListLogs)
	{
		if (!FileListInPAK.Contains(itemFile.Key))
		{
			FString PakName = FString::Printf(TEXT("%s_EngineAndTemp_0_Apk"), *m_strPlatform);
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
		UE_LOG(LogTemp, Log, TEXT("=======================SplitPaks CookedPath : FileListInPAK.Num():%d m_AllPakFileCommandsFromPakListLogs.Num() %d "), FileListInPAK.Num(), m_AllPakFileCommandsFromPakListLogs.Num());
	}

	return bResult;
}

void UCreatePaks::CreateDir(FString filePath)
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

bool UCreatePaks::ExecuteUnrealPakProcess()
{
	bool bResult = false;
	TMap<FString, TArray<FPakFileInfo>> PakFilesInfoMap;

	FString PaksDir = FString::Printf(TEXT("%sStagedBuilds/%s/%s/Content/Paks/"), *FPaths::ProjectSavedDir(), *m_strPlatform, FApp::GetProjectName());
	CreateDir(PaksDir);
	FString DLCPaksDir = FString::Printf(TEXT("%sBuildData/%s/%s/Content/Paks/"), *FPaths::ProjectDir(), *m_strPlatform, FApp::GetProjectName());
	CreateDir(DLCPaksDir);
	FString PakLogsDir = FString::Printf(TEXT("%sBuildData/%s/%s/PakLogs/"), *FPaths::ProjectDir(), *m_strPlatform, FApp::GetProjectName());
	CreateDir(PakLogsDir);

	for (auto itemPakID2AllFileDic : m_PakID2AllPakFileCommondsDic)
	{
		FString InSavePath = FString::Printf(TEXT("%sBuildData/%s/%s/PakLists/%s.txt"), *FPaths::ProjectDir(), *m_strPlatform, FApp::GetProjectName(), *itemPakID2AllFileDic.Key);
		UE_LOG(LogTemp, Log, TEXT("=======================SplitPaks InSavePath: %s."), *InSavePath);
		FString strPlatform = m_strPlatform;

		FThread CurrentPakThread(*InSavePath, [strPlatform, InSavePath, itemPakID2AllFileDic, &PakFilesInfoMap]()
			{
				FString PakName = FString::Printf(TEXT("%sStagedBuilds/%s/%s/Content/Paks/%s.pak"), *FPaths::ProjectSavedDir(), *strPlatform, FApp::GetProjectName(), *itemPakID2AllFileDic.Key);
				if (PakName.EndsWith("DLC.pak"))
				{
					PakName = FString::Printf(TEXT("%sBuildData/%s/%s/Content/Paks/%s.pak"), *FPaths::ProjectDir(), *strPlatform, FApp::GetProjectName(), *itemPakID2AllFileDic.Key);
				}

				UE_LOG(LogTemp, Log, TEXT("=======================SplitPaks PakName: %s."), *PakName);
				FString PakFileListName = FString::Printf(TEXT("%sBuildData/%s/%s/PakLists/%s.txt"), *FPaths::ProjectDir(), *strPlatform, FApp::GetProjectName(), *itemPakID2AllFileDic.Key);
				UE_LOG(LogTemp, Log, TEXT("=======================SplitPaks PakFileListName: %s."), *PakFileListName);
				FString PakLOGName = FString::Printf(TEXT("%sBuildData/%s/%s/PakLogs/%s.log"), *FPaths::ProjectDir(), *strPlatform, FApp::GetProjectName(), *itemPakID2AllFileDic.Key);
				UE_LOG(LogTemp, Log, TEXT("=======================SplitPaks PakLOGName: %s."), *PakLOGName);
				FString CryptoName = FString::Printf(TEXT("%sCooked/%s/%s/Metadata/Crypto.json"), *FPaths::ProjectSavedDir(), *strPlatform, FApp::GetProjectName());
				UE_LOG(LogTemp, Log, TEXT("=======================SplitPaks CryptoName: %s."), *CryptoName);
				FString CookerOpenOrderName = FString::Printf(TEXT("%sBuild/%s/FileOpenOrder/CookerOpenOrder.log"), *FPaths::ProjectDir(), *strPlatform);
				UE_LOG(LogTemp, Log, TEXT("=======================SplitPaks CookerOpenOrderName: %s."), *CookerOpenOrderName);
				FString CommandLine;

				if (strPlatform.Equals("WindowsNoEditor"))
				{
					/// <summary>
					/// -encryptindex -sign -patchpaddingalign=2048 -platform=Windows -compressionformats= -multiprocess
					/// </summary>
					/// <param name="strPlatform"></param>
					/// <returns></returns>
					CommandLine = FString::Printf(
						TEXT("%s -create=\"%s\" -cryptokeys=\"%s\" -order=\"%s\" -patchpaddingalign=2048 -platform=Windows -compressionformats= -multiprocess -abslog=\"%s\""),
						*PakName, *PakFileListName, *CryptoName, *CookerOpenOrderName, *PakLOGName);
					UE_LOG(LogTemp, Log, TEXT("=======================SplitPaks CommandLine: %s."), *CommandLine);
				}

				if (strPlatform.Equals("Android_ASTC"))
				{
					CommandLine = FString::Printf(
						TEXT("%s -create=\"%s\" -cryptokeys=\"%s\" -order=\"%s\" -encryptindex -platform=Android -compressionformats= -multiprocess -abslog=\"%s\""),
						*PakName, *PakFileListName, *CryptoName, *CookerOpenOrderName, *PakLOGName);
					UE_LOG(LogTemp, Log, TEXT("=======================SplitPaks CommandLine: %s."), *CommandLine);
				}


				ExecuteUnrealPak(*CommandLine);

				if (FPaths::FileExists(*InSavePath))
				{
					FPakFileInfo CurrentPakInfo;
					if (UFlibPatchParserHelper::GetPakFileInfo(InSavePath, CurrentPakInfo))
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
	}

	WritePakFilesInfoMapToJsons(PakFilesInfoMap);

	WriteFile2PakDictToJson("AllAsset2PakID.json");

	WriteScene2PaksDictToJson("AllScene2PakIDs.json");

	return bResult;
}

bool UCreatePaks::WritePakFilesInfoMapToJsons(TMap<FString, TArray<FPakFileInfo>> PakFilesInfoMap)
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
			OutString = "";
			RootJsonObj = MakeShareable(new FJsonObject);
		}
		bRunStatus = true;
	}

	return bRunStatus;
}

bool UCreatePaks::GetAllPakFiles()
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

bool UCreatePaks::CanCreatePaks()
{
	bool bCanExport = true;
	return bCanExport;
}

void UCreatePaks::CleanData()
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

}
bool UCreatePaks::DoCreatePaks()
{
	float AmountOfWorkProgress = 4.0f;
	UE_LOG(LogTemp, Log, TEXT("=======================KDLCPatcher is DoCreatePaks."));

	TArray<FString> InFilterPackagePaths = TArray<FString>{ "/Game" };
	TArray<EAssetRegistryDependencyTypeEx> AssetRegistryDependencyTypes = m_CreatePaksSettings->GetAssetRegistryDependencyTypes();//TArray<EAssetRegistryDependencyTypeEx>{ EAssetRegistryDependencyTypeEx::All };
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
	for (const auto& Platform : m_CreatePaksSettings->GetPakTargetPlatforms())
	{
		CleanData();

		AllPlatforms.AddUnique(UFlibPatchParserHelper::GetEnumNameByValue(Platform));

		m_logsFilePath = m_CreatePaksSettings->GetLogsFilePath();
		m_strPlatform = UFlibPatchParserHelper::GetEnumNameByValue(Platform);//m_strPlatform;//"WindowsNoEditor";
		UE_LOG(LogTemp, Log, TEXT("#############m_logsFilePath : %s."), *m_logsFilePath);
		UE_LOG(LogTemp, Log, TEXT("#############m_strPlatform : %s."), *m_strPlatform);

		FString BuildData = FString::Printf(TEXT("%sBuildData/%s"), *FPaths::ProjectDir(), *m_strPlatform);
		CreateDir(BuildData);

		FString AssetsDependencyString;
		//UFlibPatchParserHelper::SerializeAssetsRelatedInfoAsString(m_AssetsDependency, AssetsDependencyString);
		AssetsDependencyString = UFlibPatchParserHelper::SerializeAssetsDependencyAsJsonString(m_AssetsDependency);
		FString SaveAssetRelatedInfoToFile = FString::Printf(TEXT("%s/%s/Pacakage_AssetRelatedInfos.json"), *BuildData, FApp::GetProjectName());



		FillDirectAndRecursionDependence();

		GetAllPakFiles();
		//SplitPaksbyAPKandDLC(strPlatform);
		SpiltPakByResourceType();

	}

	return true;
}
