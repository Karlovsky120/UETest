// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemUtilsPrivatePCH.h"
#include "TestVoice.h"
#include "TestVoiceData.h"

#include "Engine.h"
#include "OnlineSubsystemUtils.h"
#include "Voice.h"

FTestVoice::FTestVoice() :
	VoiceComp(NULL),
	VoiceCapture(NULL),
	VoiceEncoder(NULL),
	VoiceDecoder(NULL),
	bUseTestSample(false),
	bZeroInput(false),
	bUseDecompressed(true),
	bZeroOutput(false)
{
	MaxRawCaptureDataSize = 100 * 1024;
	MaxCompressedDataSize = 50 * 1024;
	MaxUncompressedDataSize = MaxRawCaptureDataSize;
	MaxRemainderSize = 1 * 1024;
	LastRemainderSize = 0;

	RawCaptureData.AddUninitialized(MaxRawCaptureDataSize);
	CompressedData.AddUninitialized(MaxCompressedDataSize);
	UncompressedData.AddUninitialized(MaxUncompressedDataSize);
	Remainder.AddUninitialized(MaxRemainderSize);
}

FTestVoice::~FTestVoice()
{
	Shutdown();
}

void FTestVoice::Test()
{
	Init();
}

bool FTestVoice::Init()
{
	VoiceCapture = FVoiceModule::Get().CreateVoiceCapture();
	VoiceEncoder = FVoiceModule::Get().CreateVoiceEncoder();
	VoiceDecoder = FVoiceModule::Get().CreateVoiceDecoder();
	
	if (VoiceCapture.IsValid())
	{
		VoiceCapture->Start();
	}
	
	return true;
}

void FTestVoice::Shutdown()
{
	CompressedData.Empty();
	UncompressedData.Empty();

	if (VoiceCapture.IsValid())
	{
		VoiceCapture->Shutdown();
		VoiceCapture = NULL;
	}

	VoiceEncoder = NULL;
	VoiceDecoder = NULL;

	if (VoiceComp != NULL)
	{
		VoiceComp->RemoveFromRoot();
		VoiceComp = NULL;
	}
}

void FTestVoice::SetStaticVoiceData(TArray<uint8>& VoiceData, uint32& TotalVoiceBytes)
{
	static bool bTimeToQueue = true;
	static double LastQueueTime = 0;
	double CurrentTime = FPlatformTime::Seconds();

	if (LastQueueTime > 0 && CurrentTime - LastQueueTime > 2)
	{
		bTimeToQueue = true;
	}

	if (bTimeToQueue)
	{
		TotalVoiceBytes = ARRAY_COUNT(RawVoiceTestData);

		VoiceData.Empty(TotalVoiceBytes);
		VoiceData.AddUninitialized(TotalVoiceBytes);

		FMemory::Memcpy(VoiceData.GetTypedData(), RawVoiceTestData, ARRAY_COUNT(RawVoiceTestData));

		LastQueueTime = CurrentTime;
		bTimeToQueue = false;
	}
}

bool FTestVoice::Tick(float DeltaTime)
{
	if (VoiceCapture.IsValid())
	{
		if (!IsRunningDedicatedServer() && VoiceComp == NULL)
		{
			VoiceComp = CreateVoiceAudioComponent(VOICE_SAMPLE_RATE);
			VoiceComp->AddToRoot();
			VoiceComp->Play();
		}

		if (VoiceComp)
		{
			static bool bLastHasData = true;
			USoundWaveStreaming* SoundStreaming = CastChecked<USoundWaveStreaming>(VoiceComp->Sound);
			bool bHasData = SoundStreaming->GetAvailableAudioByteCount() != 0;

			if (bHasData != bLastHasData)
			{
				//UE_LOG(LogVoice, Log, TEXT("VOIP audio component %s starved!"), bHasData ? TEXT("is not") : TEXT("is"));
				bLastHasData = bHasData;
			}

			bool bDoWork = false;
			uint32 TotalVoiceBytes = 0;

			if (bUseTestSample)
			{
				SetStaticVoiceData(RawCaptureData, TotalVoiceBytes);
				bDoWork = true;
			}
			else
			{
				uint32 NewVoiceDataBytes = 0;
				EVoiceCaptureState::Type MicState = VoiceCapture->GetCaptureState(NewVoiceDataBytes);
				if (MicState == EVoiceCaptureState::Ok && NewVoiceDataBytes > 0)
				{
					//UE_LOG(LogVoice, Log, TEXT("Getting data! %d"), NewVoiceDataBytes);
					TotalVoiceBytes = NewVoiceDataBytes + LastRemainderSize;
					RawCaptureData.Empty(MaxRawCaptureDataSize);
					RawCaptureData.AddUninitialized(TotalVoiceBytes);

					if (LastRemainderSize > 0)
					{
						FMemory::Memcpy(RawCaptureData.GetTypedData(), Remainder.GetTypedData(), LastRemainderSize);
					}

					EVoiceCaptureState::Type MicState = VoiceCapture->GetVoiceData(RawCaptureData.GetTypedData() + LastRemainderSize, NewVoiceDataBytes, NewVoiceDataBytes);
					TotalVoiceBytes = NewVoiceDataBytes + LastRemainderSize;
					bDoWork = MicState == EVoiceCaptureState::Ok;
				}
			}

			if (bDoWork && TotalVoiceBytes > 0)
			{
				// ZERO INPUT
				if (bZeroInput)
				{
					FMemory::Memzero(RawCaptureData.GetTypedData(), TotalVoiceBytes);
				}	
				// ZERO INPUT END

				// COMPRESSION BEGIN
				uint32 CompressedDataSize = MaxCompressedDataSize;
				LastRemainderSize = VoiceEncoder->Encode(RawCaptureData.GetTypedData(), TotalVoiceBytes, CompressedData.GetTypedData(), CompressedDataSize);

				if (LastRemainderSize > 0)
				{
					FMemory::Memcpy(Remainder.GetTypedData(), RawCaptureData.GetTypedData() + (TotalVoiceBytes - LastRemainderSize), LastRemainderSize);
				}
				// COMPRESION END

				// DECOMPRESION BEGIN
				uint32 UncompressedDataSize = MaxUncompressedDataSize;
				VoiceDecoder->Decode(CompressedData.GetTypedData(), CompressedDataSize, 
					UncompressedData.GetTypedData(), UncompressedDataSize);
				// DECOMPRESSION END

				if (bUseDecompressed)
				{
					if (UncompressedDataSize > 0)
					{
						//UE_LOG(LogVoice, Log, TEXT("Queueing uncompressed data! %d"), UncompressedDataSize);
						if (bZeroOutput)
						{
							FMemory::Memzero((uint8*)UncompressedData.GetData(), UncompressedDataSize);
						}

						SoundStreaming->QueueAudio(UncompressedData.GetTypedData(), UncompressedDataSize);
					}
				}
				else
				{
					//UE_LOG(LogVoice, Log, TEXT("Queueing raw data! %d"), TotalVoiceBytes - LastRemainderSize);
					SoundStreaming->QueueAudio(RawCaptureData.GetTypedData(), TotalVoiceBytes - LastRemainderSize);
				}
			}

		}
	}

	return true;
}

bool FTestVoice::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	bool bWasHandled = false;
	return bWasHandled;
}
