// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Interfaces/IHttpResponse.h"
#include "HttpModule.h"
#include "IPlatformFilePak.h"
#include "MobilePatchingUtils/Private/MobilePatchingLibrary.h"
#include "TimerManager.h"
#include "Containers/Ticker.h"
#include "Misc/Crc.h"
#include "DlcDownloadMgr.generated.h"

//namespace PakDownloadMgrInfo
//{
	enum PakType
	{
		PakNone = 0,
		DLC = 1,
		HotPatch = 2,
	};

	//USTRUCT()
	struct FPakMessage
	{
		//GENERATED_BODY()
		FString PakName;
		uint32 PakCrc;
		PakType Type;
		int32 PakSize;//字节数，当pak过大时，对其进行拆分下载，便于断点续传
		bool hasDownloaded;//是否下载完成
	};
//}

//	struct FChunkNum
//	{
//		uint32 ChunkNum = 0;
//	};

//using namespace PakDownloadMgrInfo;

USTRUCT()
struct FVersionInfo
{
	GENERATED_BODY()
		int major;
	int minor;

	TMap<FString, FPakMessage> pakMaps;//pak,pakfilemd5

	void LoadVersion(FString& file);
	void LoadVersionByString(FString& context);
	void SaveVersion(FString& file);

	bool operator < (const FVersionInfo & b);
	bool operator == (const FVersionInfo & b);
};

//DECLARE_DELEGATE_SixParams(FOnContinueDownload, FPakMessage&, int32, int32, int32, TSharedPtr<FChunkNum,ESPMode::ThreadSafe>, int32);
/**
 * 
 */
UCLASS()
class KDLC_API UDlcDownloadMgr : public UObject
{
	GENERATED_BODY()
public:
	//使用UE的单例类，需要在工程设置Engine->GeneralSettings->DefaultClasses 设置Game Singleton Class	
	UFUNCTION(BlueprintCallable, Category = "UDlcDownloadMgr")
		static UDlcDownloadMgr* GetInstance();

	//初始化 url根节点，下载存储根节点
	UFUNCTION(BlueprintCallable, Category = "UDlcDownloadMgr")
		void Init(FString urlRoot/*, FString downloadRoot*/);

	UFUNCTION(BlueprintCallable, Category = "UDlcDownloadMgr")
		void Shutdown();

	//获取pak的绝对路径，mount时使用，当返回值为flase时pak不存在，返回值为true时方可使用
	UFUNCTION(BlueprintCallable, Category = "UDlcDownloadMgr")
		bool GetPakPath(FString pakName, FString& outPakPath);

	//是否下载完成
	UFUNCTION(BlueprintCallable, Category = "UDlcDownloadMgr")
		bool IsDownloadFinish();

	//测试用，测试当前pak的一些属性
	UFUNCTION(BlueprintCallable, Category = "UDlcDownloadMgr")
		void TestPakByName(FString pakName);

	//拉取版本信息
	UFUNCTION(BlueprintCallable, Category = "UDlcDownloadMgr")
		void UpdateVersionInfo(FString url);

	//挂载pak
	UFUNCTION(BlueprintCallable, Category = "UDlcDownloadMgr")
		void MountPakAllDownloaded();
protected:
	//下载dlc通过蓝图接口
	UFUNCTION(BlueprintCallable, Category = "UDlcDownloadMgr")
		void DownloadDlcByBP(const FString& manifest, const FString& cloudUrl, const FString& installDir);

	//挂载dlc通过蓝图接口
	UFUNCTION(BlueprintCallable, Category = "UDlcDownloadMgr")
		void MountDlcByBP(const FString& installDir);

	UFUNCTION(BlueprintInternalUseOnly, Category = "UDlcDownloadMgr")
		void OnRequestContentSucceeded(UMobilePendingContent* MobilePendingContent);
	UFUNCTION(BlueprintInternalUseOnly, Category = "UDlcDownloadMgr")
		void OnRequestContentFailed(FText ErrorText, int32 ErrorCode);
	UFUNCTION(BlueprintInternalUseOnly, Category = "UDlcDownloadMgr")
		void OnContentInstallSucceeded();
	UFUNCTION(BlueprintInternalUseOnly, Category = "UDlcDownloadMgr")
		void OnContentInstallFailed(FText ErrorText, int32 ErrorCode);

	void _InitDlcDownloadBPLib();

	void _DlcUpdateCallback(UMobilePendingContent* MobilePendingContent);
private:
	//请求manifest文件回调函数
	FOnRequestContentSucceeded onRequestContentSucceeded;
	FOnRequestContentFailed onRequestContentFailed;
	//下载安装回调函数
	FOnContentInstallSucceeded onContentInstallSucceeded;
	FOnContentInstallFailed onContentInstallFailed;
	bool _bInitDlcDownloadBPLib;
	//dlc的pak可能较大，需要更新进度等通知
	FTimerHandle dlcUpdateHandle;
protected:
	virtual void BeginDestroy() override;
	// 更新需要下载的pak列表
	bool Tick(float DeltaTime);

	void StartDownload(FPakMessage& pakName);

	static void OnRequestComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSucceeded, FPakMessage pakFile);

	void StartDownloadBigPak(FPakMessage& pakName, int32 nAllSize, int32 nBeginPos, int32 nEndPos,int32 Index,int32 NeedPakNum);

	static void OnRequestBigPakComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSucceeded, FPakMessage pakFile, int32 nAllSize, int32 nBeginPos, int32 nEndPos ,int32 Index, int32 NeedPakNum);
	//下载失败，将正在下载的 队列信息移除，加入下载队列末尾
	static void ReAdd2DownloadList(FPakMessage& pakFile);
	static void ReAdd2BigDownloadList(FString& PakName, int32 Index);

	//版本信息获取回调
	static void OnRequestVersionInfoComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSucceeded/*, FVersionInfo* _localVersion, FVersionInfo* _serverVersion*/);

	//获取本地下载目录所有pak
	bool GetAllPaksOnDownloadDir(TArray<FString> & outList);

	//将s_AllDownloadList列表数据分类填充s_PakDownloadList 和 s_DlcDownloadList 
	static void FillDownloadList();

	//static bool  CalculateDigest(uint32& crcValue, FString path);
private:
	static const int nMaxDownload = 4;//允许最大同时下载数
	static const int nMaxDlcDownload = 1;//一次只允许一个dlc下载
	static const int nMaxBigPakDownload = 4;//大文件允许最大同时下载数（与nMaxDownload按照4：1叠加）
	//FOnContinueDownload continueDownload;
	static const int32 nSpiltChunkSize = 4194304;//大的pak，会分成4M一块一块的下载
	static const int32 nNeedSpiltThreshold = 10485760;//当大于10M的pak，会被认为分块
private:
	/** Delegate for callbacks to Tick */
	FTickerDelegate TickDelegate;
	/** Handle to various registered delegates */
	FDelegateHandle TickDelegateHandle;

	FPakPlatformFile* s_pakPlatformFile;

	//static TArray<FString> s_PakDownloadList;//待下载pak
	//static TArray<FString> s_PakDownloadingList;//正在下载的pak
	//static TArray<FString> s_PakDownloadedList;//完成下载的pak

	static TMap<FString, FPakMessage> s_PakDownloadList;//待下载pak，热更pak
	static TMap<FString, FPakMessage> s_DlcDownloadList;//待下载dlcpak
	static TMap<FString, FPakMessage> s_PakDownloadingList;//正在下载的pak，热更pak
	static TMap<FString, FPakMessage> s_DlcDownloadingList;//正在下载的dlc
	static TMap<FString, FPakMessage> s_AllDownloadList;//从配置表读取，包含dlc和热更pak
	static FString s_DownloadPath;//下载根目录
	FString s_UrlRoot;//下载链接根目录
	bool b_Init;

	const int nPatchOrder = 1000;//将热更的patch 挂载权重提升
	const int nDlcOrder = 500;//dlc挂载权重
	//避免回调函数传参过多，版本信息的几个变量都声明为静态变量
	static FString localFile;
	static FString serverFile;
	static FVersionInfo f_LocalVersion;//本地版本
	static FVersionInfo f_ServerVersion;//服务端版本
	FVersionInfo* tmpVersion;//当前临时版本
	TArray<FString> s_pakPaths;//挂载的pak列表	

	//TSharedPtr<FChunkNum, ESPMode::ThreadSafe> m_ChunkNum;
	//static TMap<FString, int32> m_ChunkNum;
	//static TSharedPtr<TMap<FString, int32>> Ptr_ChunkNum = MakeShareable(&m_ChunkNum);

	static TMap<FString, TArray<int32>> s_BigPakDownloadList;//待下载的大文件列表，根据文件名key,分块index列表做value
	static TMap<FString, TArray<int32>> s_BigPakDownloadingList;
};
