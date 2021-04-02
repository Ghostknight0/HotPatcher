// Fill out your copyright notice in the Description page of Project Settings.


#include "DlcDownloadMgr.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonTypes.h"
#include "Serialization/JsonReader.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/PlatformFilemanager.h"
#include "Engine.h"
#include "PakFileUtils.h"
#if WITH_EDITOR
#include "Editor/EditorEngine.h"
#include "Editor/UnrealEdEngine.h"
#else
#include "Engine/GameEngine.h"
#endif


UGameInstance* GetGameInstance()
{
	UGameInstance* GameInstance = nullptr;
#if WITH_EDITOR
	UUnrealEdEngine* engine = Cast<UUnrealEdEngine>(GEngine);
	if (engine && engine->PlayWorld) GameInstance = engine->PlayWorld->GetGameInstance();
#else
	UGameEngine* engine = Cast<UGameEngine>(GEngine);
	if (engine) GameInstance = engine->GameInstance;
#endif
	return GameInstance;
}

struct FPakMessage;
//struct FChunkNum;


TMap<FString, FPakMessage>  UDlcDownloadMgr::s_PakDownloadList;//待下载pak
TMap<FString, FPakMessage>  UDlcDownloadMgr::s_PakDownloadingList;//正在下载的pak
TMap<FString, FPakMessage>  UDlcDownloadMgr::s_AllDownloadList;//所有需要下载的内容
TMap<FString, FPakMessage>  UDlcDownloadMgr::s_DlcDownloadList;
TMap<FString, FPakMessage>  UDlcDownloadMgr::s_DlcDownloadingList;
FString UDlcDownloadMgr::s_DownloadPath;
FString UDlcDownloadMgr::localFile;
FString UDlcDownloadMgr::serverFile;
FVersionInfo UDlcDownloadMgr::f_LocalVersion;
FVersionInfo UDlcDownloadMgr::f_ServerVersion;

TMap<FString, TArray<int32>> UDlcDownloadMgr::s_BigPakDownloadList;
TMap<FString, TArray<int32>> UDlcDownloadMgr::s_BigPakDownloadingList;
//TMap<FString, int32> UDlcDownloadMgr::m_ChunkNum;


void FVersionInfo::LoadVersion(FString & file)
{
	FString fileContext;
	FFileHelper::LoadFileToString(fileContext, *file);
	LoadVersionByString(fileContext);
}

void FVersionInfo::LoadVersionByString(FString & context)
{
	if (context.IsEmpty())
		return;

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(context);
	//将文件中的内容变成你需要的数据格式  
	if (FJsonSerializer::Deserialize(Reader, JsonObject))
	{
		auto version = JsonObject->GetStringField("Version");

		FString delim = ".";
		TArray<FString> value;
		int l2 = version.ParseIntoArray(value, delim.GetCharArray().GetData(), true);
		if (l2 == 2)
		{
			major = FCString::Atoi(*value[0]);
			minor = FCString::Atoi(*value[1]);
		}

		pakMaps.Empty();
		const TArray<TSharedPtr<FJsonValue>> Files = JsonObject->GetArrayField("Paks");
		for (int i = 0; i < Files.Num(); i++)
		{
			const TSharedPtr<FJsonObject>* PakMessageObject;
			if (Files[i].Get()->TryGetObject(PakMessageObject))
			{
				FPakMessage PakMes;
				PakMes.PakName = PakMessageObject->Get()->GetStringField("Pak");
				PakMes.PakCrc = (uint32)PakMessageObject->Get()->GetNumberField("CRC");
				PakMes.Type = (PakType)PakMessageObject->Get()->GetIntegerField("Type");
				PakMes.hasDownloaded = PakMessageObject->Get()->GetBoolField("Downloaded");
				PakMes.PakSize = (PakType)PakMessageObject->Get()->GetIntegerField("Size");
				pakMaps.Add(PakMes.PakName, PakMes);
			}
		}
		return;
	}
	else
	{
		UE_LOG(LogClass, Error, TEXT("cant prase json data,Json data could ..."));
		return;
	}
}

void FVersionInfo::SaveVersion(FString & file)
{
	TSharedPtr<FJsonObject> rootObj = MakeShareable(new FJsonObject());
	FString version = FString::FromInt(major) + "." + FString::FromInt(minor);
	rootObj->SetStringField("Version", version);

	TArray<TSharedPtr<FJsonValue>> Paks;
	for (auto & i : pakMaps)
	{
		TSharedPtr<FJsonObject> t_insideObj = MakeShareable(new FJsonObject);
		t_insideObj->SetStringField("Pak", i.Key);
		t_insideObj->SetNumberField("CRC", i.Value.PakCrc);
		t_insideObj->SetNumberField("Type", i.Value.Type);
		t_insideObj->SetBoolField("Downloaded", i.Value.hasDownloaded);
		t_insideObj->SetNumberField("Size", i.Value.PakSize);
		TSharedPtr<FJsonValueObject> tmp = MakeShareable(new FJsonValueObject(t_insideObj));
		Paks.Add(tmp);
	}
	rootObj->SetArrayField("Paks", Paks);
	FString jsonStr;
	TSharedRef<TJsonWriter<TCHAR>> jsonWriter = TJsonWriterFactory<TCHAR>::Create(&jsonStr);
	FJsonSerializer::Serialize(rootObj.ToSharedRef(), jsonWriter);
	FFileHelper::SaveStringToFile(jsonStr, *file);
}

bool FVersionInfo::operator<(const FVersionInfo & b)
{
	if (major != b.major)
		return major < b.major;
	if (minor != b.minor)
		return minor < b.minor;
	return false;
}

bool FVersionInfo::operator==(const FVersionInfo & b)
{
	return major == b.major && minor == b.minor;
}


UDlcDownloadMgr* UDlcDownloadMgr::GetInstance()
{
	static UDlcDownloadMgr* singletonObject = Cast<UDlcDownloadMgr>(GEngine->GameSingleton);
	return singletonObject;
}

void UDlcDownloadMgr::Init(FString urlRoot/*, FString downloadRoot*/)
{
	if (b_Init)
		return;
	s_DownloadPath = FPaths::ProjectPersistentDownloadDir() + "/Paks/";
	s_UrlRoot = urlRoot;
	//全局保留一个FPakPlatformFile，避免其他插件或者库有操作创建FPakPlatformFile 从而导致不是相同的FPakPlatformFile，不能Unmount pak的不利因素 
	IPlatformFile* PakFileMgr = FPlatformFileManager::Get().GetPlatformFile(FPakPlatformFile::GetTypeName());
	if (!PakFileMgr)
	{
		UE_LOG(LogTemp, Error, TEXT("FPakPlatformFile error Init Failed !"));
		b_Init = false;
		return;
	}
	s_pakPlatformFile = (FPakPlatformFile*)PakFileMgr;
	//continueDownload.BindStatic(this, &UPakDownloadMgr::StartDownloadBigPak);
	//continueDownload.BindUObject(this, &UDlcDownloadMgr::StartDownloadBigPak);
	// Register delegate for ticker callback
	TickDelegate = FTickerDelegate::CreateUObject(this, &UDlcDownloadMgr::Tick);
	TickDelegateHandle = FTicker::GetCoreTicker().AddTicker(TickDelegate);
	serverFile = FPaths::ProjectPersistentDownloadDir() + "/serverversion.json";
	localFile = FPaths::ProjectPersistentDownloadDir() + "/localversion.json";
	
	b_Init = true;
}

void UDlcDownloadMgr::Shutdown()
{
	FTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);
}

bool UDlcDownloadMgr::GetPakPath(FString pakName, FString & outPakPath)
{
	bool result = false;
	//IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	outPakPath = s_DownloadPath + pakName;
	/*if (PlatformFile.FileExists(*outPakPath))
		result = true;*/
	if (s_pakPlatformFile && s_pakPlatformFile->FileExists(*outPakPath))
		result = true;
	return result;
}

bool UDlcDownloadMgr::IsDownloadFinish()
{
	return s_PakDownloadList.Num() == 0 ? true : false;
}

void UDlcDownloadMgr::TestPakByName(FString pakName)
{
}

void UDlcDownloadMgr::UpdateVersionInfo(FString url)
{
	TSharedRef<class IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->OnProcessRequestComplete().BindStatic(UDlcDownloadMgr::OnRequestVersionInfoComplete);
	HttpRequest->SetURL(*url);
	HttpRequest->SetVerb(TEXT("GET"));
	HttpRequest->ProcessRequest();
}

void UDlcDownloadMgr::MountPakAllDownloaded()
{
	if (!s_pakPlatformFile)
	{
		UE_LOG(LogTemp, Log, TEXT("GetPlatformFile(TEXT(\"s_pakPlatformFile\") is NULL"));
		return;
	}
	TArray<FString> pakPaths;
	if (!GetAllPaksOnDownloadDir(pakPaths))
	{
		UE_LOG(LogTemp, Log, TEXT("not paks need mount"));
		return;
	}
	for (auto &i : pakPaths)
	{
		if (s_pakPaths.Contains(i))
			continue;
		//auto pakPath = s_DownloadPath + i;
		if (s_pakPlatformFile->Mount(*i, 1, nullptr))
		{
			s_pakPaths.Add(i);
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("Mount failed : %s"), *i);
		}
	}
}

void UDlcDownloadMgr::DownloadDlcByBP(const FString & manifest, const FString & cloudUrl, const FString & installDir)
{
	if (!_bInitDlcDownloadBPLib)
		_InitDlcDownloadBPLib();
	UMobilePatchingLibrary::RequestContent(manifest, cloudUrl, installDir, onRequestContentSucceeded, onRequestContentFailed);
}

void UDlcDownloadMgr::MountDlcByBP(const FString & installDir)
{
	if (!_bInitDlcDownloadBPLib)
		_InitDlcDownloadBPLib();
	auto ptr = UMobilePatchingLibrary::GetInstalledContent(installDir);
	if (ptr != nullptr)
	{
		ptr->Mount(1, "");
	}
}

void UDlcDownloadMgr::OnRequestContentSucceeded(UMobilePendingContent * MobilePendingContent)
{
	if (MobilePendingContent != nullptr)
	{
		//空间足够 进行下载安装
		if (MobilePendingContent->GetRequiredDiskSpace() < MobilePendingContent->GetDiskFreeSpace())
		{
			MobilePendingContent->StartInstall(onContentInstallSucceeded, onContentInstallFailed);

			auto instance = GetGameInstance();
			if (instance)
			{
				FTimerManager& TimerManager = instance->GetTimerManager();
				FTimerDelegate Delegate;
				const float Rate = 0.5f;

				Delegate.BindUObject(this, &UDlcDownloadMgr::_DlcUpdateCallback, MobilePendingContent);
				TimerManager.SetTimer(dlcUpdateHandle, Delegate, Rate, true);
			}		
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("UDlcDownloadMgr Disk Space Not Enough"));
		}
	}
}

void UDlcDownloadMgr::OnRequestContentFailed(FText ErrorText, int32 ErrorCode)
{
	UE_LOG(LogTemp, Error, TEXT("UDlcDownloadMgr ManifestRequestFailed %s,error code:%d"), *ErrorText.ToString(), ErrorCode);
	//重新准备下载
	auto iter = s_DlcDownloadingList.CreateIterator();
	if (iter)
	{
		s_DlcDownloadList.Add(iter->Key, iter->Value);
		s_DlcDownloadingList.Remove(iter->Key);
	}
}

void UDlcDownloadMgr::OnContentInstallSucceeded()
{
	//下载完成后，清除更新计时器
	auto instance = GetGameInstance();
	if (instance)
	{
		FTimerManager& TimerManager = instance->GetTimerManager();		
		TimerManager.ClearTimer(dlcUpdateHandle);
	}
	//下载完成后
	auto iter = s_DlcDownloadingList.CreateIterator();
	if (iter)
	{
		f_LocalVersion.pakMaps[iter->Value.PakName].hasDownloaded = true;//更新下载状态
		f_LocalVersion.SaveVersion(localFile);
		s_DlcDownloadingList.Remove(iter->Key);
	}
}

void UDlcDownloadMgr::OnContentInstallFailed(FText ErrorText, int32 ErrorCode)
{
	UE_LOG(LogTemp, Error, TEXT("UDlcDownloadMgr InstallFailed %s,error code:%d"), *ErrorText.ToString(), ErrorCode);
	//重新准备下载
	auto iter = s_DlcDownloadingList.CreateIterator();
	if (iter)
	{
		s_DlcDownloadList.Add(iter->Key, iter->Value);
		s_DlcDownloadingList.Remove(iter->Key);
	}
}

void UDlcDownloadMgr::_InitDlcDownloadBPLib()
{
	if (_bInitDlcDownloadBPLib)
		return;
	UE_LOG(LogTemp, Warning, TEXT("UDlcDownloadMgr Init Begin"));

	onRequestContentSucceeded.BindUFunction(this, FName("OnRequestContentSucceeded"));
	onRequestContentFailed.BindUFunction(this, FName("OnRequestContentFailed"));
	onContentInstallSucceeded.BindUFunction(this, FName("OnContentInstallSucceeded"));
	onContentInstallFailed.BindUFunction(this, FName("OnContentInstallFailed"));
}

//更新下载进度等事项，这里可以去通知ui
void UDlcDownloadMgr::_DlcUpdateCallback(UMobilePendingContent* MobilePendingContent)
{
	//TODO
}

void UDlcDownloadMgr::BeginDestroy()
{
	Super::BeginDestroy();
	if (b_Init)
		f_LocalVersion.SaveVersion(localFile);
}

bool UDlcDownloadMgr::Tick(float DeltaTime)
{
	if (s_PakDownloadingList.Num() < nMaxDownload && s_PakDownloadList.Num())
	{
		auto url = s_PakDownloadList.CreateIterator();
		s_PakDownloadingList.Add(url->Key, url->Value);
		if (url->Value.PakSize >= nNeedSpiltThreshold)
		{
			//获取大文件分块的总数量
			int32 chunkNum = FMath::CeilToInt((double)url->Value.PakSize / (double)nSpiltChunkSize);
			TArray<FString> Files;
			FString Extension = url->Key + "_*.temp";
			IFileManager::Get().FindFiles(Files, *s_DownloadPath, *Extension);
			TArray<int32> tmp;
			//TODO：针对已经下载完全,未拼接的temp文件，做拼接处理
			if (!!Files.Num())
			{
				//找出哪个编号文件没有，则将编号和文件名放入待下载大文件列表
				FString file;
				FString fileTemp;
				for (int i = 0; i < chunkNum; i++)
				{
					file = url->Value.PakName;
					fileTemp = FString::Printf(TEXT("%s_%d.temp"), *file, i);
					if (!Files.Contains(fileTemp))
					{
						UE_LOG(LogTemp, Error, TEXT("fileTemp need download! fileTemp = %s"),*fileTemp);

						//m_ChunkNum.Add(url->Value.PakName, 0);
						//TSharedPtr<TMap<FString, int32>> ChunkNum = MakeShareable(&m_ChunkNum);
						if (s_BigPakDownloadingList.Contains(url->Key) && s_BigPakDownloadingList[url->Key].Contains(i))
						{
							continue;
						}

						if (!s_BigPakDownloadList.Contains(url->Key))
						{
							tmp = {i};
							s_BigPakDownloadList.Add(url->Key, tmp);
						}
						else
						{
							s_BigPakDownloadList[url->Key].AddUnique(i);
						}

						//int32 begin = i * nSpiltChunkSize;
						//int32 end = (i == chunkNum - 1) ? url->Value.PakSize - 1 : (i + 1) * nSpiltChunkSize - 1;
						//StartDownloadBigPak(url->Value, url->Value.PakSize, begin, end, ChunkNum, i, (chunkNum - Files.Num()));
					}
				}
			}
			else
			{
				//FChunkNum chk;
				//FChunkNum* Chunk = &chk;
				//m_ChunkNum.Add(url->Value.PakName, 0);

				//TSharedPtr<TMap<FString, int32>> ChunkNum = MakeShareable(&m_ChunkNum);

				for (int i = 0; i < chunkNum;i++)
				{
					if (s_BigPakDownloadingList.Contains(url->Key) && s_BigPakDownloadingList[url->Key].Contains(i))
					{
						continue;
					}

					if (!s_BigPakDownloadList.Contains(url->Key))
					{
						tmp = { i };
						s_BigPakDownloadList.Add(url->Key, tmp);
					}
					else
					{
						s_BigPakDownloadList[url->Key].AddUnique(i);
					}

					//int32 begin = i * nSpiltChunkSize;
					//int32 end = (i == chunkNum - 1) ? url->Value.PakSize - 1 : (i + 1) * nSpiltChunkSize - 1;
					//StartDownloadBigPak(url->Value, url->Value.PakSize, begin, end, ChunkNum, i, chunkNum);
				}
			}

			//检查是否有temp文件，有则接着下载
			//auto file = s_DownloadPath + url->Key;
			//auto fileTemp = file + ".temp";
			//if (FPaths::FileExists(fileTemp))
			//{
			//	int32 fileSize = IFileManager::Get().FileSize(*fileTemp);
			//	int32 begin = fileSize;
			//	int32 end = url->Value.PakSize - 1;
			//	if ((url->Value.PakSize - begin) > nSpiltChunkSize)
			//		end = begin + nSpiltChunkSize - 1;
			//	UE_LOG(LogTemp, Error, TEXT("re load http range %d-%d!"), begin, end);
			//	if (end <= url->Value.PakSize - 1)
			//		StartDownloadBigPak(url->Value, url->Value.PakSize, begin, end);
			//}
			//else
			//	StartDownloadBigPak(url->Value, url->Value.PakSize, 0, nSpiltChunkSize - 1);
		}
		else
			StartDownload(url->Value);
		s_PakDownloadList.Remove(url->Key);
	}
	if (s_DlcDownloadingList.Num() < nMaxDlcDownload && s_DlcDownloadList.Num())
	{
		auto iter = s_DlcDownloadList.CreateIterator();
		s_DlcDownloadingList.Add(iter->Key, iter->Value);
		
		//StartDownload(url->Value);
		auto manifest = s_UrlRoot + "CloudDir/" + iter->Key;
		auto clouddir = s_UrlRoot + "CloudDir/";
		auto installDir = FString("DLC/") + iter->Key;
		DownloadDlcByBP(manifest,clouddir,installDir);
		s_DlcDownloadList.Remove(iter->Key);
	}

	int32 BigPakDownloadingNum = 0;
	int32 BigPakDownloadNum = 0;

	auto GetDownLoadNums = [](const TMap<FString,TArray<int32>>& downloadlist,int32& OutDownloadNum) {
		for (const auto& downloading : downloadlist)
		{
			for (const auto& downloadingValue : downloading.Value)
			{
				OutDownloadNum += 1;
			}
		}
	};

	GetDownLoadNums(s_BigPakDownloadingList, BigPakDownloadingNum);
	GetDownLoadNums(s_BigPakDownloadList, BigPakDownloadNum);

	if ((BigPakDownloadingNum < nMaxBigPakDownload) && (BigPakDownloadNum > 0))
	{
		auto iter_bigdownload = s_BigPakDownloadList.CreateIterator();

		TArray<FString> Files;
		FString Extension = iter_bigdownload->Key + "_*.temp";
		IFileManager::Get().FindFiles(Files, *s_DownloadPath, *Extension);

		FPakMessage url_Value = s_PakDownloadingList[iter_bigdownload->Key];
		int32 chunkNum = FMath::CeilToInt((double)url_Value.PakSize / (double)nSpiltChunkSize);

		int32 NeedPakNum = chunkNum;
		auto iter_bigdownloadIndex = iter_bigdownload->Value.CreateIterator();
		int32 index = iter_bigdownloadIndex.GetIndex();
		int32 i = iter_bigdownload->Value[index];
		if (!s_BigPakDownloadingList.Contains(iter_bigdownload->Key))
		{
			TArray<int32> tmp = { i };
			s_BigPakDownloadingList.Add(iter_bigdownload->Key, tmp);
		}
		else
		{
			s_BigPakDownloadingList[iter_bigdownload->Key].AddUnique(i);
		}

		//TSharedPtr<TMap<FString, int32>> ChunkNum = MakeShareable(&m_ChunkNum);
		int32 begin = i * nSpiltChunkSize;
		int32 end = (i == chunkNum - 1) ? url_Value.PakSize - 1 : (i + 1) * nSpiltChunkSize - 1;
		StartDownloadBigPak(url_Value, url_Value.PakSize, begin, end, i, NeedPakNum);

		iter_bigdownloadIndex.RemoveCurrent();
		//数组已清空，则删除s_BigPakDownloadList中的整个元素
		if (!iter_bigdownload->Value.Num())
		{
			s_BigPakDownloadList.Remove(iter_bigdownload->Key);
		}
	}

	return true;
}

void UDlcDownloadMgr::StartDownload(FPakMessage & pakName)
{
	TSharedRef<class IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->OnProcessRequestComplete().BindStatic(UDlcDownloadMgr::OnRequestComplete, pakName);
	auto url = s_UrlRoot + pakName.PakName;
	HttpRequest->SetURL(*url);
	HttpRequest->SetVerb(TEXT("GET"));
	HttpRequest->ProcessRequest();
}

void UDlcDownloadMgr::OnRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSucceeded, FPakMessage pakFile)
{
	if (!Request.IsValid() || !Response.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("Request or Response IsValid!"));
		//将正在下载的 队列信息移除，再次加入到 待下载队列末尾
		ReAdd2DownloadList(pakFile);
		return;
	}
	if (!bSucceeded)
	{
		UE_LOG(LogTemp, Error, TEXT("download fail!"));
		//将正在下载的 队列信息移除，再次加入到 待下载队列末尾
		ReAdd2DownloadList(pakFile);
		return;
	}
	int32 ResponseCode = Response->GetResponseCode();
	if (!EHttpResponseCodes::IsOk(ResponseCode))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to download manifest. ResponseCode = %d, ResponseMsg = %s"), ResponseCode, *Response->GetContentAsString());
		//将正在下载的 队列信息移除，再次加入到 待下载队列末尾
		ReAdd2DownloadList(pakFile);
		return;
	}
	if (bSucceeded && EHttpResponseCodes::IsOk(Response->GetResponseCode()))
	{
		//保存pak
		uint32 crc = 0;
		crc = CalculateDigestEx(Response->GetContent(), Response->GetContentLength());
		if (crc == pakFile.PakCrc)
		{	
			//将正在下载的 队列信息移除
			if (nullptr != s_PakDownloadingList.Find(pakFile.PakName))
			{
				s_PakDownloadingList.Remove(pakFile.PakName);
				//s_PakDownloadedList.Add(pakFile.PakName, pakFile);
				f_LocalVersion.pakMaps[pakFile.PakName].hasDownloaded = true;//更新下载状态
				f_LocalVersion.SaveVersion(localFile);
			}
			//保存pak	
			FString Filename = FPaths::GetCleanFilename(Request->GetURL());
			auto file = s_DownloadPath + pakFile.PakName;
			FFileHelper::SaveArrayToFile(Response->GetContent(), *file);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("The File(%s) CRC Calculate Failed crc = %d,version crc = %d"), *pakFile.PakName,crc, pakFile.PakCrc);
		}
	}
}

void UDlcDownloadMgr::StartDownloadBigPak(FPakMessage & pakName, int32 nAllSize, int32 nBeginPos, int32 nEndPos,int32 Index,int32 NeedPakNum)
{
	TSharedRef<class IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	auto isEnd = nEndPos == nAllSize;
	//TSharedRef<FChunkNum> ChunkNumReference = ChunkNum.ToSharedRef();
	HttpRequest->OnProcessRequestComplete().BindStatic(UDlcDownloadMgr::OnRequestBigPakComplete, pakName, nAllSize, nBeginPos, nEndPos, Index, NeedPakNum);
	auto url = s_UrlRoot + pakName.PakName;
	HttpRequest->SetURL(*url);
	FString head = FString::Printf(TEXT("bytes=%d-%d"), nBeginPos, nEndPos);
	HttpRequest->SetHeader(TEXT("Range"), head);
	HttpRequest->SetVerb(TEXT("GET"));
	HttpRequest->ProcessRequest();
}

void UDlcDownloadMgr::OnRequestBigPakComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSucceeded, FPakMessage pakFile, int32 nAllSize, int32 nBeginPos, int32 nEndPos, int32 Index, int32 NeedPakNum)
{
	if (!Request.IsValid() || !Response.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("Request or Response IsValid!"));
		ReAdd2DownloadList(pakFile);
		ReAdd2BigDownloadList(pakFile.PakName,Index);
		return;
	}
	if (!bSucceeded)
	{
		UE_LOG(LogTemp, Error, TEXT("download fail! PakName = %s"),*pakFile.PakName);
		ReAdd2DownloadList(pakFile);
		ReAdd2BigDownloadList(pakFile.PakName, Index);
		return;
	}
	int32 ResponseCode = Response->GetResponseCode();

	FString ResponseHeader = Response->GetHeader(TEXT("Content-Range"));
	UE_LOG(LogTemp, Error, TEXT("download manifest. ResponseHeader = %s"), *ResponseHeader);

	if (!EHttpResponseCodes::IsOk(ResponseCode))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to download manifest. ResponseCode = %d, ResponseMsg = %s"), ResponseCode, *Response->GetContentAsString());
		ReAdd2DownloadList(pakFile);
		ReAdd2BigDownloadList(pakFile.PakName, Index);
		return;
	}
	if (bSucceeded && EHttpResponseCodes::IsOk(Response->GetResponseCode()))
	{
		bool isEndPart = (nAllSize - 1) == nEndPos;
		UE_LOG(LogTemp, Error, TEXT("The File(%s) allsize = %d,nrnfpos = %d"), *pakFile.PakName, nAllSize, nEndPos);
		//保存pak
		FString Filename = FPaths::GetCleanFilename(Request->GetURL());
		auto file = s_DownloadPath + pakFile.PakName;
		//auto fileTemp = file + ".temp";
		auto fileTemp = FString::Printf(TEXT("%s_%d.temp"), *file, Index);
		//当断点续传时存储需要加上 FILEWRITE_Append
		//FFileHelper::SaveArrayToFile(Response->GetContent(), *fileTemp, &IFileManager::Get(), FILEWRITE_Append);
		FFileHelper::SaveArrayToFile(Response->GetContent(), *fileTemp);

		//int32 num = m_ChunkNum[pakFile.PakName];
		//num++;
		//m_ChunkNum.Add(pakFile.PakName, num);
		TArray<FString> Files;
		FString Extension = pakFile.PakName + "_*.temp";
		IFileManager::Get().FindFiles(Files, *s_DownloadPath, *Extension);

		s_BigPakDownloadingList[pakFile.PakName].Remove(Index);

		if (Files.Num() >= NeedPakNum)
		{
			UE_LOG(LogTemp, Error, TEXT("FINISH DOWNLOAD TEMP FILES!!! FILE_NAME = %s"), *pakFile.PakName);

			//拼接temp文件
			TArray<uint8> File_Content;
			for (int32 index = 0; index < Files.Num(); index++)
			{
				auto file_name = FString::Printf(TEXT("%s_%d.temp"), *file, index);

				IPlatformFile& platFormFile = FPlatformFileManager::Get().GetPlatformFile();
				IFileHandle* fileHandle = platFormFile.OpenRead(*file_name);
				fileHandle->SeekFromEnd();
				int32 len1 = fileHandle->Tell();
				UE_LOG(LogTemp, Error, TEXT("fileHandle!!! len1 = %d"), len1);
				delete fileHandle;

				FFileHelper::LoadFileToArray(File_Content, *file_name);
				if (FFileHelper::SaveArrayToFile(File_Content, *file, &IFileManager::Get(), FILEWRITE_Append))
				{
					IFileManager::Get().Delete(*file_name);
				}
			}

			uint32 crc = 0;
			if (CalculateDigest(crc, file) && crc == pakFile.PakCrc)
			{
				//IFileManager::Get().Move(*file, *fileTemp);
				if (nullptr != s_PakDownloadingList.Find(pakFile.PakName))
				{
					s_PakDownloadingList.Remove(pakFile.PakName);
					f_LocalVersion.pakMaps[pakFile.PakName].hasDownloaded = true;//更新下载状态
					f_LocalVersion.SaveVersion(localFile);

					s_BigPakDownloadingList.Remove(pakFile.PakName);
				}
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("The File(%s) CRC Calculate Failed crc = %d,version crc = %d"), *pakFile.PakName, crc, pakFile.PakCrc);
			}
		}

		//下载完成去掉后缀
		/*
		if (isEndPart)
		{
			uint32 crc = 0;
			if (CalculateDigest(crc, fileTemp) && crc == pakFile.PakCrc)
			{
				IFileManager::Get().Move(*file, *fileTemp);
				if (nullptr != s_PakDownloadingList.Find(pakFile.PakName))
				{
					s_PakDownloadingList.Remove(pakFile.PakName);				
					f_LocalVersion.pakMaps[pakFile.PakName].hasDownloaded = true;//更新下载状态
					f_LocalVersion.SaveVersion(localFile);
				}
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("The File(%s) CRC Calculate Failed crc = %d,version crc = %d"), *pakFile.PakName, crc, pakFile.PakCrc);
			}
		}
		else
		{
			if (nEndPos < nAllSize - 1)
			{
				int32 begin = nEndPos + 1;
				int32 end = nAllSize - 1;
				if ((nAllSize - nEndPos) > nSpiltChunkSize)
					end = begin + nSpiltChunkSize - 1;
				UE_LOG(LogTemp, Error, TEXT("http range %d-%d!"), begin, end);
				continueDownload.ExecuteIfBound(pakFile, nAllSize, begin, end);
			}
		}*/
	}
}

void UDlcDownloadMgr::ReAdd2DownloadList(FPakMessage & pakFile)
{
	if (nullptr != s_PakDownloadingList.Find(pakFile.PakName))
	{
		s_PakDownloadingList.Remove(pakFile.PakName);
		s_PakDownloadList.Add(pakFile.PakName, pakFile);
	}
}

void UDlcDownloadMgr::ReAdd2BigDownloadList(FString& PakName,int32 Index)
{
	if (nullptr != s_BigPakDownloadingList.Find(PakName))
	{
		s_BigPakDownloadingList[PakName].Remove(Index);
		if (!s_BigPakDownloadingList[PakName].Num())
		{
			s_BigPakDownloadingList.Remove(PakName);
		}

		if (nullptr != s_BigPakDownloadList.Find(PakName))
		{
			s_BigPakDownloadList[PakName].AddUnique(Index);
		}
		else
		{
			TArray<int32> tmp = { Index };
			s_BigPakDownloadList.Add(PakName, tmp);
		}
	}
}

void UDlcDownloadMgr::OnRequestVersionInfoComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSucceeded)
{
	if (!Request.IsValid() || !Response.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("version info request or response isvalid !"));
		return;
	}
	if (!bSucceeded)
	{
		UE_LOG(LogTemp, Error, TEXT("version info download fail!"));
		return;
	}
	int32 ResponseCode = Response->GetResponseCode();
	if (!EHttpResponseCodes::IsOk(ResponseCode))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to download version info . ResponseCode = %d, ResponseMsg = %s"), ResponseCode, *Response->GetContentAsString());
		return;
	}
	if (bSucceeded && EHttpResponseCodes::IsOk(Response->GetResponseCode()))
	{
		FString Filename = FPaths::GetCleanFilename(Request->GetURL());
		FFileHelper::SaveArrayToFile(Response->GetContent(), *serverFile);
		
		s_PakDownloadingList.Empty();
		f_ServerVersion.LoadVersion(serverFile);
		if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*localFile))
		{
			f_LocalVersion.LoadVersion(localFile);
			if (f_LocalVersion < f_ServerVersion)
			{
				//差异比较pak的file，填充s_PakDownloadList列表
				TArray<FString> localKeys;
				f_LocalVersion.pakMaps.GetKeys(localKeys);
				TArray<FString> serverKeys;
				f_ServerVersion.pakMaps.GetKeys(serverKeys);

				int size1 = localKeys.Num();
				int size2 = serverKeys.Num();
				int end = localKeys.Num();
				bool swap = false;//标识变量，表示两种情况中的哪一种
				for (int i = 0; i < end;)
				{
					swap = false;//开始假设是第一种情况
					for (int j = i; j < size2; j++)//找到与该元素存在相同的元素，将这个相同的元素交换到与该元素相同下标的位置上
					{
						if (localKeys[i] == serverKeys[j])//第二种情况，找到了相等的元素
						{
							FString tmp = serverKeys[i];//对数组2进行交换
							serverKeys[i] = serverKeys[j];
							serverKeys[j] = tmp;
							swap = true;//设置标志
							break;
						}
					}
					if (swap != true)//第一种情况，没有相同元素存在时，将这个元素交换到尚未进行比较的尾部
					{
						FString tmp = localKeys[i];
						localKeys[i] = localKeys[--end];
						localKeys[end] = tmp;
					}
					else
						i++;
				}

				s_PakDownloadList.Empty();
				s_AllDownloadList.Empty();
				s_DlcDownloadList.Empty();
				s_BigPakDownloadList.Empty();
				//在本地版本信息中，没在服务端但却没下载的，需要加入下载列表
				for (int i = end; i < size1; i++)
				{
					if (!f_LocalVersion.pakMaps[localKeys[i]].hasDownloaded)
						//s_PakDownloadList.Add(localKeys[i]);
						//s_PakDownloadList.Add(localKeys[i], f_LocalVersion.pakMaps[localKeys[i]]);//
					s_AllDownloadList.Add(localKeys[i], f_LocalVersion.pakMaps[localKeys[i]]);
				}

				//在本地版本信息，但是和服务端版本md5不匹配，需要加入下载列表
				TArray<FString> updateKeys;
				for (int i = 0; i < end; i++)
				{
					updateKeys.Add(localKeys[i]);
					UE_LOG(LogTemp, Error, TEXT("version pak update {%s}!"), *localKeys[i]);
				}

				for (auto & i : updateKeys)
				{
					if (f_LocalVersion.pakMaps[i].PakCrc != f_ServerVersion.pakMaps[i].PakCrc)
						//s_PakDownloadList.Add(i);//将需更新的本地pak加入下载列表
						//s_PakDownloadList.Add(i, f_LocalVersion.pakMaps[i]);
						s_AllDownloadList.Add(i, f_LocalVersion.pakMaps[i]);
				}
				for (int i = end; i < size2; i++)
				{
					//s_PakDownloadList.Add(serverKeys[i]);	
					//s_PakDownloadList.Add(serverKeys[i], f_ServerVersion.pakMaps[serverKeys[i]]);
					s_AllDownloadList.Add(serverKeys[i], f_ServerVersion.pakMaps[serverKeys[i]]);
				}
				FillDownloadList();
			}
			else
			{
				s_PakDownloadList.Empty();
				s_AllDownloadList.Empty();
				s_DlcDownloadList.Empty();
				s_BigPakDownloadList.Empty();
				for (auto &element : f_LocalVersion.pakMaps)
				{
					if (!element.Value.hasDownloaded)
						//s_PakDownloadList.Add(element.Key, element.Value);
						s_AllDownloadList.Add(element.Key, element.Value);
				}
				FillDownloadList();
			}
		}
		else
		{
			f_LocalVersion.LoadVersion(serverFile);
			f_LocalVersion.SaveVersion(localFile);
			s_PakDownloadList.Empty();
			s_AllDownloadList.Empty();
			s_DlcDownloadList.Empty();
			s_BigPakDownloadList.Empty();
			for (auto i : f_LocalVersion.pakMaps)
				//s_PakDownloadList.Add(i.Key, i.Value);
				s_AllDownloadList.Add(i.Key, i.Value);
			FillDownloadList();
		}
	}
}

void UDlcDownloadMgr::FillDownloadList()
{
	for (auto &i : s_AllDownloadList)
	{
		if (i.Value.Type == PakType::DLC)
		{
			s_DlcDownloadList.Add(i.Key, i.Value);
		}
		else if (i.Value.Type == PakType::HotPatch)
		{
			s_PakDownloadList.Add(i.Key, i.Value);
		}
	}
}

bool UDlcDownloadMgr::GetAllPaksOnDownloadDir(TArray<FString>& outList)
{
	FString suffix = ".pak";

	auto path = s_DownloadPath;
	if (s_pakPlatformFile)
	{
		s_pakPlatformFile->FindFilesRecursively(outList, *path, *suffix);
		if (outList.Num())
			return true;
		else
			return false;
	}
	else
		return false;
}
