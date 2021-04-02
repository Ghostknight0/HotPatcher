#pragma once
#include "CoreMinimal.h"
#include "Misc/Crc.h"
#include "CoreTypes.h"
#include "HAL/Platform.h"
//crc 文件校验
bool  CalculateDigest(uint32& crcValue, FString path)
{
	/*FCrc crc;
	crc.Init();
	FILE* file = fopen(TCHAR_TO_UTF8(*path), "r");
	if (!file)
	{
		UE_LOG(LogStreaming, Warning, TEXT("Failed to read file '%s' error."), *path);
		return false;
	}
	const int len = 1024;
	char buffer[len];
	uint32 value = 0;
	while (true)
	{
		int read = fread(buffer, sizeof(char), len, file);
		if (read == 0)
			break;
		value = crc.MemCrc32(buffer, (uint32)read, value);
	}
	crcValue = value;
	fclose(file);
	return true;*/

	//需要使用智能指针回收
	TSharedPtr<FArchive, ESPMode::ThreadSafe> Ar = MakeShareable(IFileManager::Get().CreateFileReader(*path));
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

//目前只对小文件pak进行校验，即patch包的进行校验，dlc的不校验
uint32  CalculateDigestEx(const TArray<uint8>& array, const uint32 len)
{
	uint32 crcValue = 0;
	FCrc crc;
	crc.Init();
	crcValue = crc.MemCrc32(array.GetData(), len);

	return crcValue;
}