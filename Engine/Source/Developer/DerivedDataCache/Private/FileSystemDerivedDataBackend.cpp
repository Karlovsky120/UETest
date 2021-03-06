// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.


#include "Core.h"

#include "DerivedDataBackendInterface.h"
#include "DDCCleanup.h"

#define MAX_BACKEND_KEY_LENGTH (120)
#define MAX_BACKEND_NUMBERED_SUBFOLDER_LENGTH (9)
#define MAX_CACHE_DIR_LEN (119)
#define MAX_CACHE_EXTENTION_LEN (4)

/** 
 * Cache server that uses the OS filesystem
 * The entire API should be callable from any thread (except the singleton can be assumed to be called at least once before concurrent access).
**/
class FFileSystemDerivedDataBackend : public FDerivedDataBackendInterface
{
	/* Scoped timer that warns if the DDC access is taking too long. */
	class FSlowAccessWarning
	{
		const FFileSystemDerivedDataBackend& Backend;
		double StartTime;
	public:
		FSlowAccessWarning(const FFileSystemDerivedDataBackend& InBackend)
			: Backend(InBackend)
		{
			StartTime = FPlatformTime::Seconds();
		}
		~FSlowAccessWarning()
		{			
			double Duration = FPlatformTime::Seconds() - StartTime;
			const double SlowDuration = 10.0;
			if (Duration >= SlowDuration)
			{
				UE_LOG(LogDerivedDataCache, Warning, TEXT("%s access is very slow, consider disabling it."), *Backend.CachePath);
			}
		}
	};
public:
	/**
	 * Constructor that should only be called once by the singleton, grabs the cache path from the ini
	 * @param InCacheDirectory	directory to store the cache in
	 * @param bForceReadOnly	if true, do not attempt to write to this cache
	*/
	FFileSystemDerivedDataBackend(const TCHAR* InCacheDirectory, bool bForceReadOnly, bool bTouchFiles, bool bPurgeTransientData, bool bDeleteOldFiles, int32 InDaysToDeleteUnusedFiles, int32 InMaxNumFoldersToCheck, int32 InMaxContinuousFileChecks)
		: CachePath(InCacheDirectory)
		, bReadOnly(bForceReadOnly)
		, bFailed(true)
		, bTouch(bTouchFiles)
		, bPurgeTransient(bPurgeTransientData)
		, DaysToDeleteUnusedFiles(InDaysToDeleteUnusedFiles)
	{
		// If we find a platform that has more stingent limits, this needs to be rethought.
		checkAtCompileTime(MAX_BACKEND_KEY_LENGTH + MAX_CACHE_DIR_LEN + MAX_BACKEND_NUMBERED_SUBFOLDER_LENGTH + MAX_CACHE_EXTENTION_LEN < PLATFORM_MAX_FILEPATH_LENGTH, not_enough_room_left_for_cache_keys_in_max_path);
		const double SlowInitDuration = 10.0;
		bool bFilesystemIsSlow = false;

		check(CachePath.Len());
		FPaths::NormalizeFilename(CachePath);
		const FString AbsoluteCachePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*CachePath);
		if (AbsoluteCachePath.Len() > MAX_CACHE_DIR_LEN)
		{
			const FText ErrorMessage = FText::Format( NSLOCTEXT("DerivedDataCache", "PathTooLong", "Cache path {0} is longer than {1} characters...please adjust [DerivedDataBackendGraph] paths to be shorter (this leaves more room for cache keys)."), FText::FromString( AbsoluteCachePath ), FText::AsNumber( MAX_CACHE_DIR_LEN ) );
			FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage);
			UE_LOG(LogDerivedDataCache, Fatal, TEXT("%s"), *ErrorMessage.ToString());
		}
		if (!bReadOnly)
		{
			double TestStart = FPlatformTime::Seconds();
			FString TempFilename = CachePath / FGuid::NewGuid().ToString() + ".tmp";
			FFileHelper::SaveStringToFile(FString("TEST"),*TempFilename);
			int32 TestFileSize = IFileManager::Get().FileSize(*TempFilename);
			if (TestFileSize < 4)
			{
				UE_LOG(LogDerivedDataCache, Warning, TEXT("Fail to write to %s, derived data cache to this directory will be read only."),*CachePath);
			}
			else
			{
				bFailed = false;
			}			
			if (TestFileSize >= 0)
			{
				IFileManager::Get().Delete(*TempFilename, false, false, true);
			}
			double TestDuration = FPlatformTime::Seconds() - TestStart;
			if (TestDuration > SlowInitDuration)
			{
				bFilesystemIsSlow = true;
			}
		}
		if (bFailed)
		{
			double StartTime = FPlatformTime::Seconds();
			TArray<FString> FilesAndDirectories;
			IFileManager::Get().FindFiles(FilesAndDirectories,*(CachePath / TEXT("*.*")), true, true);
			double TestDuration = FPlatformTime::Seconds() - StartTime;
			if (FilesAndDirectories.Num() > 0)
			{
				bReadOnly = true;
				bFailed = false;
			}
			if (TestDuration > SlowInitDuration)
			{
				bFilesystemIsSlow = true;
			}
		}
		if (FString(FCommandLine::Get()).Contains(TEXT("DerivedDataCache")) )
		{
			bTouch = true; // we always touch files when running the DDC commandlet
		}
		// The command line (-ddctouch) enables touch on all filesystem backends if specified. 
		bTouch = bTouch || FParse::Param(FCommandLine::Get(), TEXT("DDCTOUCH"));

		if (bReadOnly)
		{
			bTouch = false; // we won't touch read only paths
		}

		if (bTouch)
		{
			UE_LOG(LogDerivedDataCache, Display, TEXT("Files in %s will be touched."),*CachePath);
		}

		if (!bFailed && bFilesystemIsSlow)
		{
			UE_LOG(LogDerivedDataCache, Warning, TEXT("%s access is very slow, consider disabling it."), *CachePath);
			uint32 Result = FMessageDialog::Open(EAppMsgType::YesNo, FText::Format(NSLOCTEXT("Engine", "SlowDDC", "DDC filesystem backend is very slow:\n\n    {0}\n\nWould you like to disable it?"), FText::FromString(CachePath)));
			if (Result == EAppReturnType::Yes)
			{
				bFailed = true;
			}
		}

		if (!bReadOnly && !bFailed && bDeleteOldFiles && !FParse::Param(FCommandLine::Get(),TEXT("NODDCCLEANUP")) && FDDCCleanup::Get())
		{			
			FDDCCleanup::Get()->AddFilesystem( CachePath, InDaysToDeleteUnusedFiles, InMaxNumFoldersToCheck, InMaxContinuousFileChecks );
		}
	}

	/** return true if the cache is usable **/
	bool IsUsable()
	{
		return !bFailed;
	}

	/** return true if this cache is writable **/
	virtual bool IsWritable()
	{
		return !bReadOnly;
	}
	/**
	 * Synchronous test for the existence of a cache item
	 *
	 * @param	CacheKey	Alphanumeric+underscore key of this cache item
	 * @return				true if the data probably will be found, this can't be guaranteed because of concurrency in the backends, corruption, etc
	 */
	virtual bool CachedDataProbablyExists(const TCHAR* CacheKey)
	{
		check(!bFailed);
		FString Filename = BuildFilename(CacheKey);
		bool bExists = FPaths::FileExists(Filename);
		if (bExists)
		{
			// Update file timestamp to prevent it from being deleted by DDC Cleanup.
			if (bTouch || 
				 (!bReadOnly && (FDateTime::UtcNow() - IFileManager::Get().GetTimeStamp(*Filename)).GetDays() > (DaysToDeleteUnusedFiles / 4)))
			{
				IFileManager::Get().SetTimeStamp(*Filename, FDateTime::UtcNow());
			}
		}
		return bExists;
	}
	/**
	 * Synchronous retrieve of a cache item
	 *
	 * @param	CacheKey	Alphanumeric+underscore key of this cache item
	 * @param	OutData		Buffer to receive the results, if any were found
	 * @return				true if any data was found, and in this case OutData is non-empty
	 */
	virtual bool GetCachedData(const TCHAR* CacheKey,TArray<uint8>& Data)
	{
		check(!bFailed);
		FString Filename = BuildFilename(CacheKey);
		FSlowAccessWarning ScopedWarning(*this);
		if (FFileHelper::LoadFileToArray(Data,*Filename,FILEREAD_Silent))
		{
			UE_LOG(LogDerivedDataCache, Verbose, TEXT("FileSystemDerivedDataBackend: Cache hit on %s"),*Filename);
			return true;
		}
		UE_LOG(LogDerivedDataCache, Verbose, TEXT("FileSystemDerivedDataBackend: Cache miss on %s"),*Filename);
		Data.Empty();
		return false;	
	}
	/**
	 * Asynchronous, fire-and-forget placement of a cache item
	 *
	 * @param	CacheKey	Alphanumeric+underscore key of this cache item
	 * @param	OutData		Buffer containing the data to cache, can be destroyed after the call returns, immediately
	 * @param	bPutEvenIfExists	If true, then do not attempt skip the put even if CachedDataProbablyExists returns true
	 */
	virtual void PutCachedData(const TCHAR* CacheKey, TArray<uint8>& Data, bool bPutEvenIfExists) OVERRIDE
	{
		check(!bFailed);
		if (!bReadOnly)
		{
			if (bPutEvenIfExists || !CachedDataProbablyExists(CacheKey))
			{
				check(Data.Num());
				FString Filename = BuildFilename(CacheKey);
				FString TempFilename(TEXT("temp.")); 
				TempFilename += FGuid::NewGuid().ToString();
				TempFilename = FPaths::GetPath(Filename) / TempFilename;
				bool bResult;
				{
					FSlowAccessWarning ScopedWarning(*this);
					bResult = FFileHelper::SaveArrayToFile(Data, *TempFilename);
				}
				if (bResult)
				{
					if (IFileManager::Get().FileSize(*TempFilename) == Data.Num())
					{
						bool DoMove = !CachedDataProbablyExists(CacheKey);
						if (bPutEvenIfExists && !DoMove)
						{
							DoMove = true;
							RemoveCachedData(CacheKey, /*bTransient=*/ false);
						}
						if (DoMove) 
						{
							if (!IFileManager::Get().Move(*Filename, *TempFilename, true, true, false, true))
							{
								UE_LOG(LogDerivedDataCache, Log, TEXT("FFileSystemDerivedDataBackend: Move collision, attempt at redundant update, OK %s."),*Filename);
							}
							else
							{
								UE_LOG(LogDerivedDataCache, Verbose, TEXT("FFileSystemDerivedDataBackend: Successful cache put to %s"),*Filename);
							}
						}
					}
					else
					{
						UE_LOG(LogDerivedDataCache, Warning, TEXT("FFileSystemDerivedDataBackend: Temp file is short %s!"),*TempFilename);
					}
				}
				else
				{
					UE_LOG(LogDerivedDataCache, Warning, TEXT("FFileSystemDerivedDataBackend: Could not write temp file %s!"),*TempFilename);
				}
				// if everything worked, this is not necessary, but we will make every effort to avoid leaving junk in the cache
				if (FPaths::FileExists(TempFilename))
				{
					IFileManager::Get().Delete(*TempFilename, false, false, true);
				}
			}
		}
	}

	void RemoveCachedData(const TCHAR* CacheKey, bool bTransient) OVERRIDE
	{
		check(!bFailed);
		if (!bReadOnly && (!bTransient || bPurgeTransient))
		{
			FString Filename = BuildFilename(CacheKey);
			if (bTransient)
			{
				UE_LOG(LogDerivedDataCache,Verbose,TEXT("Deleting transient cached data. Key=%s Filename=%s"),CacheKey,*Filename);
			}
			IFileManager::Get().Delete(*Filename, false, false, true);
		}
	}

private:

	/**
	 * Threadsafe method to compute the filename from the cachekey, currently just adds a path and an extension.
	 *
	 * @param	CacheKey	Alphanumeric+underscore key of this cache item
	 * @return				filename built from the cache key
	 */
	FString BuildFilename(const TCHAR* CacheKey)
	{
		FString Key = FString(CacheKey).ToUpper();
		for (int32 i = 0; i < Key.Len(); i++)
		{
			check(FChar::IsAlnum(Key[i]) || FChar::IsUnderscore(Key[i]) || Key[i] == L'$');
		}
		uint32 Hash = FCrc::StrCrc_DEPRECATED(*Key);
		// this creates a tree of 1000 directories
		FString HashPath = FString::Printf(TEXT("%1d/%1d/%1d/"),(Hash/100)%10,(Hash/10)%10,Hash%10);
		return CachePath / HashPath / Key + TEXT(".udd");
	}

	/** Base path we are storing the cache files in. **/
	FString	CachePath;
	/** If true, do not attempt to write to this cache **/
	bool		bReadOnly;
	/** If true, we failed to write to this directory and it did not contain anything so we should not be used **/
	bool		bFailed;
	/** If true, CachedDataProbablyExists will update the file timestamps. */
	bool		bTouch;
	/** If true, allow transient data to be removed from the cache. */
	bool		bPurgeTransient;
	/** Age of file when it should be deleted from DDC cache. */
	int32		DaysToDeleteUnusedFiles;
};

FDerivedDataBackendInterface* CreateFileSystemDerivedDataBackend(const TCHAR* CacheDirectory, bool bForceReadOnly /*= false*/, bool bTouchFiles /*= false*/, bool bPurgeTransient /*= false*/, bool bDeleteOldFiles /*= false*/, int32 InDaysToDeleteUnusedFiles /*= 60*/, int32 InMaxNumFoldersToCheck /*= -1*/, int32 InMaxContinuousFileChecks /*= -1*/)
{
	FFileSystemDerivedDataBackend* FileDDB = new FFileSystemDerivedDataBackend(CacheDirectory, bForceReadOnly, bTouchFiles, bPurgeTransient, bDeleteOldFiles, InDaysToDeleteUnusedFiles, InMaxNumFoldersToCheck, InMaxContinuousFileChecks);
	if (!FileDDB->IsUsable())
	{
		delete FileDDB;
		FileDDB = NULL;
	}
	return FileDDB;
}
