// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "EnginePrivate.h"
#include "VisualLogger/VisualLoggerBinaryFileDevice.h"

#if ENABLE_VISUAL_LOG

FVisualLoggerBinaryFileDevice::FVisualLoggerBinaryFileDevice()
	: FileArchive(NULL)
{
	Cleanup();

	bool DefaultFrameCacheLenght = 0;
	GConfig->GetBool(TEXT("VisualLogger"), TEXT("FrameCacheLenght"), DefaultFrameCacheLenght, GEngineIni);
	FrameCacheLenght = DefaultFrameCacheLenght;

	bool UseCompression = false;
	GConfig->GetBool(TEXT("VisualLogger"), TEXT("UseCompression"), UseCompression, GEngineIni);
	bUseCompression = UseCompression;
}

void FVisualLoggerBinaryFileDevice::Cleanup(bool bReleaseMemory)
{

}

void FVisualLoggerBinaryFileDevice::StartRecordingToFile(float TimeStamp)
{
	if (FileArchive != NULL)
	{
		return;
	}

	StartRecordingTime = TimeStamp;
	LastLogTimeStamp = StartRecordingTime;
	TempFileName = FVisualLoggerHelpers::GenerateTemporaryFilename(VISLOG_FILENAME_EXT);
	const FString FullFilename = FString::Printf(TEXT("%slogs/%s"), *FPaths::GameSavedDir(), *TempFileName);

	FileArchive = IFileManager::Get().CreateFileWriter(*FullFilename);
}

void FVisualLoggerBinaryFileDevice::StopRecordingToFile(float TimeStamp)
{
	if (FileArchive == NULL)
	{
		return;
	}

	const int32 NumEntries = FrameCache.Num();
	if (NumEntries> 0)
	{
		*FileArchive << FrameCache;
		FrameCache.Reset();
	}

	const int64 TotalSize = FileArchive->TotalSize();
	FileArchive->Close();
	delete FileArchive;
	FileArchive = NULL;

	const FString TempFullFilename = FString::Printf(TEXT("%slogs/%s"), *FPaths::GameSavedDir(), *TempFileName);
	const FString NewFileName = FVisualLoggerHelpers::GenerateFilename(TempFileName, FileName, StartRecordingTime, LastLogTimeStamp);

	if (TotalSize > 0)
	{
		// rename file when we serialized some data
		IFileManager::Get().Move(*NewFileName, *TempFullFilename, true);
	}
	else
	{
		// or remove file if nothing serialized
		IFileManager::Get().Delete(*TempFullFilename, false, true, true);
	}
}

void FVisualLoggerBinaryFileDevice::SetFileName(const FString& InFileName)
{
	FileName = InFileName;
}

void FVisualLoggerBinaryFileDevice::Serialize(const UObject* LogOwner, FName OwnerName, const FVisualLogEntry& LogEntry)
{
	const int32 NumEntries = FrameCache.Num();
	if (NumEntries> 0 && LastLogTimeStamp + FrameCacheLenght <= LogEntry.TimeStamp && FileArchive)
	{
		FVisualLoggerHelpers::Serialize(*FileArchive, FrameCache);
		FrameCache.Reset();
	}

	LastLogTimeStamp = LogEntry.TimeStamp;
	FrameCache.Add(FVisualLogEntryItem(OwnerName, LogEntry));
}
#endif
