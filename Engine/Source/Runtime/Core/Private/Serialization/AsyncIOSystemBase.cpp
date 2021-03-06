// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AsyncIOSystemBase.h: Base implementation of the async IO system
=============================================================================*/

#include "CorePrivate.h"
#include "AsyncIOSystemBase.h"

/*-----------------------------------------------------------------------------
	FAsyncIOSystemBase implementation.
-----------------------------------------------------------------------------*/

#define BLOCK_ON_ASYNCIO 0

// Constrain bandwidth if wanted. Value is in MByte/ sec.
float GAsyncIOBandwidthLimit = 0.0f;

CORE_API bool GbLogAsyncLoading = false;

uint64 FAsyncIOSystemBase::QueueIORequest( 
	const FString& FileName, 
	int64 Offset, 
	int64 Size, 
	int64 UncompressedSize, 
	void* Dest, 
	ECompressionFlags CompressionFlags, 
	FThreadSafeCounter* Counter,
	EAsyncIOPriority Priority )
{
	FScopeLock ScopeLock( CriticalSection );
	check( Offset != INDEX_NONE );

	// Create an IO request containing passed in information.
	FAsyncIORequest IORequest;
	IORequest.RequestIndex				= RequestIndex++;
	IORequest.FileSortKey				= INDEX_NONE;
	IORequest.FileName					= FileName;
	IORequest.Offset					= Offset;
	IORequest.Size						= Size;
	IORequest.UncompressedSize			= UncompressedSize;
	IORequest.Dest						= Dest;
	IORequest.CompressionFlags			= CompressionFlags;
	IORequest.Counter					= Counter;
	IORequest.Priority					= Priority;

	static bool HasCheckedCommandline = false;
	if (!HasCheckedCommandline)
	{
		HasCheckedCommandline = true;
		if ( FParse::Param(FCommandLine::Get(), TEXT("logasync")))
		{
			GbLogAsyncLoading = true;
			UE_LOG(LogStreaming, Warning, TEXT("*** ASYNC LOGGING IS ENABLED"));
		}
	}
	if (GbLogAsyncLoading == true)
	{
		LogIORequest(TEXT("QueueIORequest"), IORequest);
	}

	INC_DWORD_STAT( STAT_AsyncIO_OutstandingReadCount );
	INC_DWORD_STAT_BY( STAT_AsyncIO_OutstandingReadSize, IORequest.Size );

	// Add to end of queue.
	OutstandingRequests.Add( IORequest );

	// Trigger event telling IO thread to wake up to perform work.
	OutstandingRequestsEvent->Trigger();

	// Return unique ID associated with request which can be used to cancel it.
	return IORequest.RequestIndex;
}

uint64 FAsyncIOSystemBase::QueueDestroyHandleRequest(const FString& FileName)
{
	FScopeLock ScopeLock( CriticalSection );
	FAsyncIORequest IORequest;
	IORequest.RequestIndex				= RequestIndex++;
	IORequest.FileName					= FileName;
	IORequest.Priority					= AIOP_MIN;
	IORequest.bIsDestroyHandleRequest	= true;

	if (GbLogAsyncLoading == true)
	{
		LogIORequest(TEXT("QueueDestroyHandleRequest"), IORequest);
	}

	// Add to end of queue.
	OutstandingRequests.Add( IORequest );

	// Trigger event telling IO thread to wake up to perform work.
	OutstandingRequestsEvent->Trigger();

	// Return unique ID associated with request which can be used to cancel it.
	return IORequest.RequestIndex;
}

void FAsyncIOSystemBase::LogIORequest(const FString& Message, const FAsyncIORequest& IORequest)
{
	FString OutputStr = FString::Printf(TEXT("ASYNC: %32s: %s\n"), *Message, *(IORequest.ToString()));
	FPlatformMisc::LowLevelOutputDebugString(*OutputStr);
}

bool FAsyncIOSystemBase::InternalRead( IFileHandle* FileHandle, int64 Offset, int64 Size, void* Dest )
{
	FScopeLock ScopeLock( ExclusiveReadCriticalSection );

	bool bRetVal = false;
	
	STAT(double ReadTime = 0);
	{	
		SCOPE_SECONDS_COUNTER(ReadTime);
		PlatformReadDoNotCallDirectly( FileHandle, Offset, Size, Dest );
	}	
	INC_FLOAT_STAT_BY(STAT_AsyncIO_PlatformReadTime,(float)ReadTime);

	// The platform might actually read more than Size due to aligning and internal min read sizes
	// though we only really care about throttling requested bandwidth as it's not very accurate
	// to begin with.
	STAT(ConstrainBandwidth(Size, ReadTime));

	return bRetVal;
}

bool FAsyncIOSystemBase::PlatformReadDoNotCallDirectly( IFileHandle* FileHandle, int64 Offset, int64 Size, void* Dest )
{
	check(FileHandle);
	if (!FileHandle->Seek(Offset))
	{
		UE_LOG(LogStreaming, Error,TEXT("Seek failure."));
		return false;
	}
	if (!FileHandle->Read((uint8*)Dest, Size))
	{
		UE_LOG(LogStreaming, Error,TEXT("Read failure."));
		return false;
	}
	return true;
}

IFileHandle* FAsyncIOSystemBase::PlatformCreateHandle( const TCHAR* FileName )
{
	return LowLevel.OpenRead(FileName);
}


int32 FAsyncIOSystemBase::PlatformGetNextRequestIndex()
{
	// Find first index of highest priority request level. Basically FIFO per priority.
	int32 HighestPriorityIndex = INDEX_NONE;
	EAsyncIOPriority HighestPriority = static_cast<EAsyncIOPriority>(AIOP_MIN - 1);
	for( int32 CurrentRequestIndex=0; CurrentRequestIndex<OutstandingRequests.Num(); CurrentRequestIndex++ )
	{
		// Calling code already entered critical section so we can access OutstandingRequests.
		const FAsyncIORequest& IORequest = OutstandingRequests[CurrentRequestIndex];
		if( IORequest.Priority > HighestPriority )
		{
			HighestPriority = IORequest.Priority;
			HighestPriorityIndex = CurrentRequestIndex;
		}
	}
	return HighestPriorityIndex;
}

void FAsyncIOSystemBase::PlatformHandleHintDoneWithFile(const FString& Filename)
{
	QueueDestroyHandleRequest(Filename);
}

int64 FAsyncIOSystemBase::PlatformMinimumReadSize()
{
	return 32*1024;
}



// If enabled allows tracking down crashes in decompression as it avoids using the async work queue.
#define BLOCK_ON_DECOMPRESSION 0

void FAsyncIOSystemBase::FulfillCompressedRead( const FAsyncIORequest& IORequest, IFileHandle* FileHandle )
{
	if (GbLogAsyncLoading == true)
	{
		LogIORequest(TEXT("FulfillCompressedRead"), IORequest);
	}

	// Initialize variables.
	FAsyncUncompress*		Uncompressor			= NULL;
	uint8*					UncompressedBuffer		= (uint8*) IORequest.Dest;
	// First compression chunk contains information about total size so we skip that one.
	int32						CurrentChunkIndex		= 1;
	int32						CurrentBufferIndex		= 0;
	bool					bHasProcessedAllData	= false;

	// read the first two ints, which will contain the magic bytes (to detect byteswapping)
	// and the original size the chunks were compressed from
	int64						HeaderData[2];
	int32						HeaderSize = sizeof(HeaderData);

	InternalRead(FileHandle, IORequest.Offset, HeaderSize, HeaderData);
	RETURN_IF_EXIT_REQUESTED;

	// if the magic bytes don't match, then we are byteswapped (or corrupted)
	bool bIsByteswapped = HeaderData[0] != PACKAGE_FILE_TAG;
	// if its potentially byteswapped, make sure it's not just corrupted
	if (bIsByteswapped)
	{
		// if it doesn't equal the swapped version, then data is corrupted
		if (HeaderData[0] != PACKAGE_FILE_TAG_SWAPPED)
		{
			UE_LOG(LogStreaming, Warning, TEXT("Detected data corruption [header] trying to read %lld bytes at offset %lld from '%s'. Please delete file and recook."),
				IORequest.UncompressedSize, 
				IORequest.Offset ,
				*IORequest.FileName );
			check(0);
			FPlatformMisc::HandleIOFailure(*IORequest.FileName);
		}
		// otherwise, we have a valid byteswapped file, so swap the chunk size
		else
		{
			HeaderData[1] = BYTESWAP_ORDER64(HeaderData[1]);
		}
	}

	int32						CompressionChunkSize	= HeaderData[1];
	
	// handle old packages that don't have the chunk size in the header, in which case
	// we can use the old hardcoded size
	if (CompressionChunkSize == PACKAGE_FILE_TAG)
	{
		CompressionChunkSize = LOADING_COMPRESSION_CHUNK_SIZE;
	}

	// calculate the number of chunks based on the size they were compressed from
	int32						TotalChunkCount = (IORequest.UncompressedSize + CompressionChunkSize - 1) / CompressionChunkSize + 1;

	// allocate chunk info data based on number of chunks
	FCompressedChunkInfo*	CompressionChunks		= (FCompressedChunkInfo*)FMemory::Malloc(sizeof(FCompressedChunkInfo) * TotalChunkCount);
	int32						ChunkInfoSize			= (TotalChunkCount) * sizeof(FCompressedChunkInfo);
	void*					CompressedBuffer[2]		= { 0, 0 };
	
	// Read table of compression chunks after seeking to offset (after the initial header data)
	InternalRead( FileHandle, IORequest.Offset + HeaderSize, ChunkInfoSize, CompressionChunks );
	RETURN_IF_EXIT_REQUESTED;

	// Handle byte swapping. This is required for opening a cooked file on the PC.
	int64 CalculatedUncompressedSize = 0;
	if (bIsByteswapped)
	{
		for( int32 ChunkIndex=0; ChunkIndex<TotalChunkCount; ChunkIndex++ )
		{
			CompressionChunks[ChunkIndex].CompressedSize	= BYTESWAP_ORDER64(CompressionChunks[ChunkIndex].CompressedSize);
			CompressionChunks[ChunkIndex].UncompressedSize	= BYTESWAP_ORDER64(CompressionChunks[ChunkIndex].UncompressedSize);
			if (ChunkIndex > 0)
			{
				CalculatedUncompressedSize += CompressionChunks[ChunkIndex].UncompressedSize;
			}
		}
	}
	else
	{
		for( int32 ChunkIndex=1; ChunkIndex<TotalChunkCount; ChunkIndex++ )
		{
			CalculatedUncompressedSize += CompressionChunks[ChunkIndex].UncompressedSize;
		}
	}

	if (CompressionChunks[0].UncompressedSize != CalculatedUncompressedSize)
	{
		UE_LOG(LogStreaming, Warning, TEXT("Detected data corruption [incorrect uncompressed size] calculated %i bytes, requested %i bytes at offset %i from '%s'. Please delete file and recook."),
			CalculatedUncompressedSize,
			IORequest.UncompressedSize, 
			IORequest.Offset ,
			*IORequest.FileName );
		check(0);
		FPlatformMisc::HandleIOFailure(*IORequest.FileName);
	}

	if (ChunkInfoSize + HeaderSize + CompressionChunks[0].CompressedSize > IORequest.Size )
	{
		UE_LOG(LogStreaming, Warning, TEXT("Detected data corruption [undershoot] trying to read %lld bytes at offset %lld from '%s'. Please delete file and recook."),
			IORequest.UncompressedSize, 
			IORequest.Offset ,
			*IORequest.FileName );
		check(0);
		FPlatformMisc::HandleIOFailure(*IORequest.FileName);
	}

	if (IORequest.UncompressedSize != CalculatedUncompressedSize)
	{
		UE_LOG(LogStreaming, Warning, TEXT("Detected data corruption [incorrect uncompressed size] calculated %lld bytes, requested %lld bytes at offset %lld from '%s'. Please delete file and recook."),
			CalculatedUncompressedSize,
			IORequest.UncompressedSize, 
			IORequest.Offset ,
			*IORequest.FileName );
		check(0);
		FPlatformMisc::HandleIOFailure(*IORequest.FileName);
	}

	// Figure out maximum size of compressed data chunk.
	int64 MaxCompressedSize = 0;
	for (int32 ChunkIndex = 1; ChunkIndex < TotalChunkCount; ChunkIndex++)
	{
		MaxCompressedSize = FMath::Max(MaxCompressedSize, CompressionChunks[ChunkIndex].CompressedSize);
		// Verify the all chunks are 'full size' until the last one...
		if (CompressionChunks[ChunkIndex].UncompressedSize < CompressionChunkSize)
		{
			if (ChunkIndex != (TotalChunkCount - 1))
			{
				checkf(0, TEXT("Calculated too many chunks: %d should be last, there are %d from '%s'"), ChunkIndex, TotalChunkCount, *IORequest.FileName);
			}
		}
		check( CompressionChunks[ChunkIndex].UncompressedSize <= CompressionChunkSize );
	}

	int32 Padding = 0;

	// Allocate memory for compressed data.
	CompressedBuffer[0]	= FMemory::Malloc( MaxCompressedSize + Padding );
	CompressedBuffer[1] = FMemory::Malloc( MaxCompressedSize + Padding );

	// Initial read request.
	InternalRead( FileHandle, FileHandle->Tell(), CompressionChunks[CurrentChunkIndex].CompressedSize, CompressedBuffer[CurrentBufferIndex] );
	RETURN_IF_EXIT_REQUESTED;

	// Loop till we're done decompressing all data.
	while( !bHasProcessedAllData )
	{
		FAsyncTask<FAsyncUncompress> UncompressTask(
			IORequest.CompressionFlags,
			UncompressedBuffer,
			CompressionChunks[CurrentChunkIndex].UncompressedSize,
			CompressedBuffer[CurrentBufferIndex],
			CompressionChunks[CurrentChunkIndex].CompressedSize,
			(Padding > 0)
			);

#if BLOCK_ON_DECOMPRESSION
		UncompressTask.StartSynchronousTask();
#else
		UncompressTask.StartBackgroundTask();
#endif

		// Advance destination pointer.
		UncompressedBuffer += CompressionChunks[CurrentChunkIndex].UncompressedSize;
	
		// Check whether we are already done reading.
		if( CurrentChunkIndex < TotalChunkCount-1 )
		{
			// Can't postincrement in if statement as we need it to remain at valid value for one more loop iteration to finish
		// the decompression.
			CurrentChunkIndex++;
			// Swap compression buffers to read into.
			CurrentBufferIndex = 1 - CurrentBufferIndex;
			// Read more data.
			InternalRead( FileHandle, FileHandle->Tell(), CompressionChunks[CurrentChunkIndex].CompressedSize, CompressedBuffer[CurrentBufferIndex] );
			RETURN_IF_EXIT_REQUESTED;
		}
		// We were already done reading the last time around so we are done processing now.
		else
		{
			bHasProcessedAllData = true;
		}
		
		//@todo async loading: should use event for this
		STAT(double UncompressorWaitTime = 0);
		{
			SCOPE_SECONDS_COUNTER(UncompressorWaitTime);
			UncompressTask.EnsureCompletion(); // just decompress on this thread if it isn't started yet
		}
		INC_FLOAT_STAT_BY(STAT_AsyncIO_UncompressorWaitTime,(float)UncompressorWaitTime);
	}

	FMemory::Free(CompressionChunks);
	FMemory::Free(CompressedBuffer[0]);
	FMemory::Free(CompressedBuffer[1] );
}

IFileHandle* FAsyncIOSystemBase::GetCachedFileHandle( const FString& FileName )
{
	// We can't make any assumptions about NULL being an invalid handle value so we need to use the indirection.
	IFileHandle*	FileHandle = FindCachedFileHandle( FileName );

	// We have an already cached handle, let's use it.
	if( !FileHandle )
	{
		// So let the platform specific code create one.
		FileHandle = PlatformCreateHandle( *FileName );
		// Make sure it's valid before caching and using it.
		if( FileHandle )
		{
			NameToHandleMap.Add( *FileName, FileHandle );
		}
	}

	return FileHandle;
}

IFileHandle* FAsyncIOSystemBase::FindCachedFileHandle( const FString& FileName )
{
	return NameToHandleMap.FindRef( FileName );
}

uint64 FAsyncIOSystemBase::LoadData( 
	const FString& FileName, 
	int64 Offset, 
	int64 Size, 
	void* Dest, 
	FThreadSafeCounter* Counter,
	EAsyncIOPriority Priority )
{
	uint64 TheRequestIndex;
	{
		TheRequestIndex = QueueIORequest( FileName, Offset, Size, 0, Dest, COMPRESS_None, Counter, Priority );
	}
#if BLOCK_ON_ASYNCIO
	BlockTillAllRequestsFinished(); 
#endif
	return TheRequestIndex;
}

uint64 FAsyncIOSystemBase::LoadCompressedData( 
	const FString& FileName, 
	int64 Offset, 
	int64 Size, 
	int64 UncompressedSize, 
	void* Dest, 
	ECompressionFlags CompressionFlags, 
	FThreadSafeCounter* Counter,
	EAsyncIOPriority Priority )
{
	uint64 TheRequestIndex;
	{
		TheRequestIndex = QueueIORequest( FileName, Offset, Size, UncompressedSize, Dest, CompressionFlags, Counter, Priority );
	}
#if BLOCK_ON_ASYNCIO
	BlockTillAllRequestsFinished(); 
#endif
	return TheRequestIndex;
}

int32 FAsyncIOSystemBase::CancelRequests( uint64* RequestIndices, int32 NumIndices )
{
	FScopeLock ScopeLock( CriticalSection );

	// Iterate over all outstanding requests and cancel matching ones.
	int32 RequestsCanceled = 0;
	for( int32 OutstandingIndex=OutstandingRequests.Num()-1; OutstandingIndex>=0 && RequestsCanceled<NumIndices; OutstandingIndex-- )
	{
		// Iterate over all indices of requests to cancel
		for( int32 TheRequestIndex=0; TheRequestIndex<NumIndices; TheRequestIndex++ )
		{
			// Look for matching request index in queue.
			const FAsyncIORequest IORequest = OutstandingRequests[OutstandingIndex];
			if( IORequest.RequestIndex == RequestIndices[TheRequestIndex] )
			{
				INC_DWORD_STAT( STAT_AsyncIO_CanceledReadCount );
				INC_DWORD_STAT_BY( STAT_AsyncIO_CanceledReadSize, IORequest.Size );
				DEC_DWORD_STAT( STAT_AsyncIO_OutstandingReadCount );
				DEC_DWORD_STAT_BY( STAT_AsyncIO_OutstandingReadSize, IORequest.Size );				
				// Decrement thread-safe counter to indicate that request has been "completed".
				IORequest.Counter->Decrement();
				// IORequest variable no longer valid after removal.
				OutstandingRequests.RemoveAt( OutstandingIndex );
				RequestsCanceled++;
				// Break out of loop as we've modified OutstandingRequests AND it no longer is valid.
				break;
			}
		}
	}
	return RequestsCanceled;
}

void FAsyncIOSystemBase::CancelAllOutstandingRequests()
{
	FScopeLock ScopeLock( CriticalSection );

	// simply toss all outstanding requests - the critical section will guarantee we aren't removing
	// while using elsewhere
	OutstandingRequests.Empty();
}

void FAsyncIOSystemBase::ConstrainBandwidth( int64 BytesRead, float ReadTime )
{
	// Constrain bandwidth if wanted. Value is in MByte/ sec.
	if( GAsyncIOBandwidthLimit > 0.0f )
	{
		// Figure out how long to wait to throttle bandwidth.
		float WaitTime = BytesRead / (GAsyncIOBandwidthLimit * 1024.f * 1024.f) - ReadTime;
		// Only wait if there is something worth waiting for.
		if( WaitTime > 0 )
		{
			// Time in seconds to wait.
			FPlatformProcess::Sleep(WaitTime);
		}
	}
}

bool FAsyncIOSystemBase::Init()
{
	CriticalSection				= new FCriticalSection();
	ExclusiveReadCriticalSection= new FCriticalSection();
	OutstandingRequestsEvent	= FPlatformProcess::CreateSynchEvent();
	RequestIndex				= 1;
	MinPriority					= AIOP_MIN;
	IsRunning.Increment();
	return true;
}

void FAsyncIOSystemBase::Suspend()
{
	SuspendCount.Increment();
	ExclusiveReadCriticalSection->Lock();
}

void FAsyncIOSystemBase::Resume()
{
	ExclusiveReadCriticalSection->Unlock();
	SuspendCount.Decrement();
}

void FAsyncIOSystemBase::SetMinPriority( EAsyncIOPriority InMinPriority )
{
	FScopeLock ScopeLock( CriticalSection );
	// Trigger event telling IO thread to wake up to perform work if we are lowering the min priority.
	if( InMinPriority < MinPriority )
	{
		OutstandingRequestsEvent->Trigger();
	}
	// Update priority.
	MinPriority = InMinPriority;
}

void FAsyncIOSystemBase::HintDoneWithFile(const FString& Filename)
{
	// let the platform handle it
	PlatformHandleHintDoneWithFile(Filename);
}

int64 FAsyncIOSystemBase::MinimumReadSize()
{
	// let the platform handle it
	return PlatformMinimumReadSize();
}


void FAsyncIOSystemBase::Exit()
{
	FlushHandles();
	delete CriticalSection;
	delete OutstandingRequestsEvent;
}

void FAsyncIOSystemBase::Stop()
{
	// Tell the thread to quit.
	IsRunning.Decrement();

	// Make sure that the thread awakens even if there is no work currently outstanding.
	OutstandingRequestsEvent->Trigger();
}

uint32 FAsyncIOSystemBase::Run()
{
	// IsRunning gets decremented by Stop.
	while( IsRunning.GetValue() > 0 )
	{
		// Sit and spin if requested, unless we are shutting down, in which case make sure we don't deadlock.
		while( !GIsRequestingExit && SuspendCount.GetValue() > 0 )
		{
			FPlatformProcess::Sleep(0.005);
		}

		Tick();
	}

	return 0;
}

void FAsyncIOSystemBase::Tick()
{
	// Create file handles.
	{
		TArray<FString> FileNamesToCacheHandles; 
		// Only enter critical section for copying existing array over. We don't operate on the 
		// real array as creating file handles might take a while and we don't want to have other
		// threads stalling on submission of requests.
		{
			FScopeLock ScopeLock( CriticalSection );

			for( int32 RequestIdx=0; RequestIdx<OutstandingRequests.Num(); RequestIdx++ )
			{
				// Early outs avoid unnecessary work and string copies with implicit allocator churn.
				FAsyncIORequest& OutstandingRequest = OutstandingRequests[RequestIdx];
				if( OutstandingRequest.bHasAlreadyRequestedHandleToBeCached == false
				&&	OutstandingRequest.bIsDestroyHandleRequest == false 
				&&	FindCachedFileHandle( OutstandingRequest.FileName ) == NULL )
				{
					new(FileNamesToCacheHandles)FString(*OutstandingRequest.FileName);
					OutstandingRequest.bHasAlreadyRequestedHandleToBeCached = true;
				}
			}
		}
		// Create file handles for requests down the pipe. This is done here so we can later on
		// use the handles to figure out the sort keys.
		for( int32 FileNameIndex=0; FileNameIndex<FileNamesToCacheHandles.Num(); FileNameIndex++ )
		{
			GetCachedFileHandle( FileNamesToCacheHandles[FileNameIndex] );
		}
	}

	// Copy of request.
	FAsyncIORequest IORequest;
	bool			bIsRequestPending	= false;
	{
		FScopeLock ScopeLock( CriticalSection );
		if( OutstandingRequests.Num() )
		{
			// Gets next request index based on platform specific criteria like layout on disc.
			int32 TheRequestIndex = PlatformGetNextRequestIndex();
			if( TheRequestIndex != INDEX_NONE )
			{					
				// We need to copy as we're going to remove it...
				IORequest = OutstandingRequests[ TheRequestIndex ];
				// ...right here.
				// NOTE: this needs to be a Remove, not a RemoveSwap because the base implementation
				// of PlatformGetNextRequestIndex is a FIFO taking priority into account
				OutstandingRequests.RemoveAt( TheRequestIndex );		
				// We're busy. Updated inside scoped lock to ensure BlockTillAllRequestsFinished works correctly.
				BusyWithRequest.Increment();
				bIsRequestPending = true;
			}
		}
	}

	// We only have work to do if there's a request pending.
	if( bIsRequestPending )
	{
		// handle a destroy handle request from the queue
		if( IORequest.bIsDestroyHandleRequest )
		{
			IFileHandle*	FileHandle = FindCachedFileHandle( IORequest.FileName );
			if( FileHandle )
			{
				// destroy and remove the handle
				delete FileHandle;
				NameToHandleMap.Remove(IORequest.FileName);
			}
		}
		else
		{
			// Retrieve cached handle or create it if it wasn't cached. We purposefully don't look at currently
			// set value as it might be stale by now.
			IFileHandle* FileHandle = GetCachedFileHandle( IORequest.FileName );
			if( FileHandle )
			{
				if( IORequest.UncompressedSize )
				{
					// Data is compressed on disc so we need to also decompress.
					FulfillCompressedRead( IORequest, FileHandle );
				}
				else
				{
					// Read data after seeking.
					InternalRead( FileHandle, IORequest.Offset, IORequest.Size, IORequest.Dest );
				}
				INC_DWORD_STAT( STAT_AsyncIO_FulfilledReadCount );
				INC_DWORD_STAT_BY( STAT_AsyncIO_FulfilledReadSize, IORequest.Size );
			}
			else
			{
				//@todo streaming: add warning once we have thread safe logging.
			}

			DEC_DWORD_STAT( STAT_AsyncIO_OutstandingReadCount );
			DEC_DWORD_STAT_BY( STAT_AsyncIO_OutstandingReadSize, IORequest.Size );
		}

		// Request fulfilled.
		if( IORequest.Counter )
		{
			IORequest.Counter->Decrement(); 
		}
		// We're done reading for now.
		BusyWithRequest.Decrement();	
	}
	else
	{
		if( !OutstandingRequests.Num() && FPlatformProcess::SupportsMultithreading() )
		{
			// We're really out of requests now, wait till the calling thread signals further work
			OutstandingRequestsEvent->Wait();
		}
	}
}

void FAsyncIOSystemBase::TickSingleThreaded()
{
	// This should only be used when multithreading is disabled.
	check(FPlatformProcess::SupportsMultithreading() == false);
	Tick();
}

void FAsyncIOSystemBase::BlockTillAllRequestsFinished()
{
	// Block till all requests are fulfilled.
	while( true ) 
	{
		bool bHasFinishedRequests = false;
		{
			FScopeLock ScopeLock( CriticalSection );
			bHasFinishedRequests = (OutstandingRequests.Num() == 0) && (BusyWithRequest.GetValue() == 0);
		}	
		if( bHasFinishedRequests )
		{
			break;
		}
		else
		{
			SHUTDOWN_IF_EXIT_REQUESTED;

			//@todo streaming: this should be replaced by waiting for an event.
			FPlatformProcess::Sleep( 0.001f );
		}
	}
}

void FAsyncIOSystemBase::BlockTillAllRequestsFinishedAndFlushHandles()
{
	// Block till all requests are fulfilled.
	BlockTillAllRequestsFinished();

	// Flush all file handles.
	FlushHandles();
}

void FAsyncIOSystemBase::FlushHandles()
{
	FScopeLock ScopeLock( CriticalSection );
	// Iterate over all file handles, destroy them and empty name to handle map.
	for( TMap<FString,IFileHandle*>::TIterator It(NameToHandleMap); It; ++It )
	{
		delete It.Value();
	}
	NameToHandleMap.Empty();
}

/** Thread used for async IO manager */
static FRunnableThread*	AsyncIOThread = NULL;
static FAsyncIOSystemBase* AsyncIOSystem = NULL;

FIOSystem& FIOSystem::Get()
{
	if (!AsyncIOThread)
	{
		check(!AsyncIOSystem);
		GConfig->GetFloat( TEXT("Core.System"), TEXT("AsyncIOBandwidthLimit"), GAsyncIOBandwidthLimit, GEngineIni );
		AsyncIOSystem = FPlatformMisc::GetPlatformSpecificAsyncIOSystem();
		if (!AsyncIOSystem)
		{
			// the platform didn't have a specific need, so we just use the base class with the normal file system.
			AsyncIOSystem = new FAsyncIOSystemBase(FPlatformFileManager::Get().GetPlatformFile());
		}
		AsyncIOThread = FRunnableThread::Create(AsyncIOSystem, TEXT("AsyncIOSystem"), 0, 0, 16384, TPri_AboveNormal );
		check(AsyncIOThread);
	}
	check(AsyncIOSystem);
	return *AsyncIOSystem;
}

void FIOSystem::Shutdown()
{
	if (AsyncIOThread)
	{
		AsyncIOThread->Kill(true);
		delete AsyncIOThread;
	}
	if (AsyncIOSystem)
	{
		delete AsyncIOSystem;
		AsyncIOSystem = NULL;
	}
	AsyncIOThread = (FRunnableThread*)-1; // non null, we don't allow a restart after a shutdown as this is usually an error of some sort
}

bool FIOSystem::HasShutdown()
{
	return AsyncIOThread == (FRunnableThread*)-1;
}