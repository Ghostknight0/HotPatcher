#include "CreatePakCommandlet.h"
#include "CreatePatch/CreatePaks.h"


DEFINE_LOG_CATEGORY_STATIC(CreatePak, Log, All);

int32 UCreatePakCommandlet::Main(const FString& Params)
{
	UE_LOG(CreatePak, Display, TEXT("===========CreatePak CreatePak CreatePak!==========="));

	//FString config_path;
	//bool bStatus = FParse::Value(*Params, *FString(PATCHER_CONFIG_PARAM_NAME).ToLower(), config_path);
	//if (!bStatus)
	//{
	//	UE_LOG(LogHotPatcherCommandlet, Error, TEXT("UHotPatcherCommandlet error: not -config=xxxx.json params."));
	//	return -1;
	//}

	//if (!FPaths::FileExists(config_path))
	//{
	//	UE_LOG(LogHotPatcherCommandlet, Error, TEXT("UHotPatcherCommandlet error: cofnig file %s not exists."), *config_path);
	//	return -1;
	//}

	//FString JsonContent;
	//if (FFileHelper::LoadFileToString(JsonContent, *config_path))
	//{
		UCreatePaks* myCreatePak = NewObject<UCreatePaks>();
		myCreatePak->DoCreatePaks();
	//}
	system("pause");
	return 0;
}