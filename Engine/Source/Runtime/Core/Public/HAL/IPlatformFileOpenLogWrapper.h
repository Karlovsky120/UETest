// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#if !UE_BUILD_SHIPPING

class CORE_API FPlatformFileOpenLog : public IPlatformFile
{
protected:

	IPlatformFile*			LowerLevel;
	FCriticalSection		CriticalSection;
	int64					OpenOrder;
	TMap<FString, int64>	FilenameAccessMap;
	TArray<IFileHandle*>	LogOutput;

public:

	FPlatformFileOpenLog()
		: LowerLevel(nullptr)
		, OpenOrder(0)
	{
	}

	virtual ~FPlatformFileOpenLog()
	{
	}

	virtual bool ShouldBeUsed(IPlatformFile* Inner, const TCHAR* CmdLine) const OVERRIDE
	{
		bool bResult = FParse::Param(CmdLine, TEXT("FileOpenLog"));
		return bResult;
	}

	virtual bool Initialize(IPlatformFile* Inner, const TCHAR* CommandLineParam) OVERRIDE
	{
		LowerLevel = Inner;
		FString LogFileDirectory;
		FString LogFilePath;
		FString PlatformStr;

		if (FParse::Value(CommandLineParam, TEXT("TARGETPLATFORM="), PlatformStr))
		{
			TArray<FString> PlatformNames;
			if (!(PlatformStr == TEXT("None") || PlatformStr == TEXT("All")))
			{
				PlatformStr.ParseIntoArray(&PlatformNames, TEXT("+"), true);
			}

			for (int32 Platform = 0;Platform < PlatformNames.Num(); ++Platform)
			{
				LogFileDirectory = FPaths::Combine( FPlatformMisc::GameDir(), TEXT( "Build" ), *PlatformNames[Platform], TEXT("FileOpenOrder"));
#if WITH_EDITOR
				LogFilePath = FPaths::Combine( *LogFileDirectory, TEXT("EditorOpenOrder.log"));
#else 
				LogFilePath = FPaths::Combine( *LogFileDirectory, TEXT("GameOpenOrder.log"));
#endif
				Inner->CreateDirectoryTree(*LogFileDirectory);
				LogOutput.Add(Inner->OpenWrite(*LogFilePath, false, false));
			}
		}
		else
		{
			LogFileDirectory = FPaths::Combine( FPlatformMisc::GameDir(), TEXT( "Build" ), StringCast<TCHAR>(FPlatformProperties::PlatformName()).Get(), TEXT("FileOpenOrder"));
#if WITH_EDITOR
			LogFilePath = FPaths::Combine( *LogFileDirectory, TEXT("EditorOpenOrder.log"));
#else 
			LogFilePath = FPaths::Combine( *LogFileDirectory, TEXT("GameOpenOrder.log"));
#endif
			Inner->CreateDirectoryTree(*LogFileDirectory);
			LogOutput.Add(Inner->OpenWrite(*LogFilePath, false, false));
		}
		return true;
	}
	virtual IPlatformFile* GetLowerLevel() OVERRIDE
	{
		return LowerLevel;
	}
	static const TCHAR* GetTypeName()
	{
		return TEXT("FileOpenLog");
	}
	virtual const TCHAR* GetName() const OVERRIDE
	{
		return GetTypeName();
	}
	virtual bool		FileExists(const TCHAR* Filename) OVERRIDE
	{
		return LowerLevel->FileExists(Filename);
	}
	virtual int64		FileSize(const TCHAR* Filename) OVERRIDE
	{
		return LowerLevel->FileSize(Filename);
	}
	virtual bool		DeleteFile(const TCHAR* Filename) OVERRIDE
	{
		return LowerLevel->DeleteFile(Filename);
	}
	virtual bool		IsReadOnly(const TCHAR* Filename) OVERRIDE
	{
		return LowerLevel->IsReadOnly(Filename);
	}
	virtual bool		MoveFile(const TCHAR* To, const TCHAR* From) OVERRIDE
	{
		return LowerLevel->MoveFile(To, From);
	}
	virtual bool		SetReadOnly(const TCHAR* Filename, bool bNewReadOnlyValue) OVERRIDE
	{
		return LowerLevel->SetReadOnly(Filename, bNewReadOnlyValue);
	}
	virtual FDateTime	GetTimeStamp(const TCHAR* Filename) OVERRIDE
	{
		return LowerLevel->GetTimeStamp(Filename);
	}
	virtual void		SetTimeStamp(const TCHAR* Filename, FDateTime DateTime) OVERRIDE
	{
		LowerLevel->SetTimeStamp(Filename, DateTime);
	}
	virtual FDateTime	GetAccessTimeStamp(const TCHAR* Filename) OVERRIDE
	{
		return LowerLevel->GetAccessTimeStamp(Filename);
	}
	virtual IFileHandle*	OpenRead(const TCHAR* Filename) OVERRIDE
	{
		IFileHandle* Result = LowerLevel->OpenRead(Filename);
		if (Result)
		{
			CriticalSection.Lock();
			if (FilenameAccessMap.Find(Filename) == nullptr)
			{
				FilenameAccessMap.Emplace(Filename, ++OpenOrder);
				FString Text = FString::Printf(TEXT("\"%s\" %llu\n"), Filename, OpenOrder);
				for (auto File = LogOutput.CreateIterator(); File; ++File)
				{
					(*File)->Write((uint8*)StringCast<ANSICHAR>(*Text).Get(), Text.Len());
				}
			}
			CriticalSection.Unlock();
		}
		return Result;
	}
	virtual IFileHandle*	OpenWrite(const TCHAR* Filename, bool bAppend = false, bool bAllowRead = false) OVERRIDE
	{
		return LowerLevel->OpenWrite(Filename, bAppend, bAllowRead);
	}
	virtual bool		DirectoryExists(const TCHAR* Directory) OVERRIDE
	{
		return LowerLevel->DirectoryExists(Directory);
	}
	virtual bool		CreateDirectory(const TCHAR* Directory) OVERRIDE
	{
		return LowerLevel->CreateDirectory(Directory);
	}
	virtual bool		DeleteDirectory(const TCHAR* Directory) OVERRIDE
	{
		return LowerLevel->DeleteDirectory(Directory);
	}
	virtual bool		IterateDirectory(const TCHAR* Directory, IPlatformFile::FDirectoryVisitor& Visitor) OVERRIDE
	{
		return LowerLevel->IterateDirectory( Directory, Visitor );
	}
	virtual bool		IterateDirectoryRecursively(const TCHAR* Directory, IPlatformFile::FDirectoryVisitor& Visitor) OVERRIDE
	{
		return LowerLevel->IterateDirectoryRecursively( Directory, Visitor );
	}
	virtual bool		DeleteDirectoryRecursively(const TCHAR* Directory) OVERRIDE
	{
		return LowerLevel->DeleteDirectoryRecursively( Directory );
	}
	virtual bool		CopyFile(const TCHAR* To, const TCHAR* From) OVERRIDE
	{
		return LowerLevel->CopyFile( To, From );
	}
	virtual bool		CreateDirectoryTree(const TCHAR* Directory) OVERRIDE
	{
		return LowerLevel->CreateDirectoryTree(Directory);
	}
	virtual bool		CopyDirectoryTree(const TCHAR* DestinationDirectory, const TCHAR* Source, bool bOverwriteAllExisting) OVERRIDE
	{
		return LowerLevel->CopyDirectoryTree(DestinationDirectory, Source, bOverwriteAllExisting);
	}
	virtual FString		ConvertToAbsolutePathForExternalAppForRead( const TCHAR* Filename ) OVERRIDE
	{
		return LowerLevel->ConvertToAbsolutePathForExternalAppForRead(Filename);
	}
	virtual FString		ConvertToAbsolutePathForExternalAppForWrite( const TCHAR* Filename ) OVERRIDE
	{
		return LowerLevel->ConvertToAbsolutePathForExternalAppForWrite(Filename);
	}
	virtual bool		SendMessageToServer(const TCHAR* Message, IFileServerMessageHandler* Handler) OVERRIDE
	{
		return LowerLevel->SendMessageToServer(Message, Handler);
	}
};

#endif // !UE_BUILD_SHIPPING