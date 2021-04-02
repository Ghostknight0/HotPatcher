// Fill out your copyright notice in the Description page of Project Settings.


#include "GeneratedFileCrcCommandlet.h"
#include "PakFileUtils.h"
#include "Misc/FileHelper.h"

#define PATCHER_CONFIG_PARAM_NAME TEXT("-config=")
int32 UGeneratedFileCrcCommandlet::Main(const FString& Params)
{
	FString config_path;
	bool bStatus = FParse::Value(*Params, *FString(PATCHER_CONFIG_PARAM_NAME).ToLower(), config_path);

	if (bStatus)
	{
		FString delim = ";";
		TArray<FString> files;
		TArray<FString> filesCrc;
		int line = config_path.ParseIntoArray(files, delim.GetCharArray().GetData(), true);
		for (auto & i : files)
		{
			UE_LOG(LogStreaming, Warning, TEXT("parse to read file '%s' error."), *i);
			uint32 crc = 0;
			auto ret = CalculateDigest(crc, i);
			if (ret)
			{
				auto str = i + " = " + FString::Printf(TEXT("%u"), crc);
				filesCrc.Add(str);
			}
		}
		auto file = FPaths::ProjectDir() + "/crc.txt";
		FFileHelper::SaveStringArrayToFile(filesCrc, *file);
	}
	return 0;
}