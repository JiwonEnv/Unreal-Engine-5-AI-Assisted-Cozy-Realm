#pragma once
#include "CoreMinimal.h"
#include "Async/Future.h"

/** UObject는 게임 스레드에서 직렬화하고, 파일 쓰기만 순서대로 백그라운드에서 처리한다. */
class FCozyEstateSaveIO
{
public:
	~FCozyEstateSaveIO();
	void Initialize(FString InPath, TArray<uint8> PreviousBytes);
	bool IsInitialized() const { return !Path.IsEmpty(); }
	void Enqueue(TArray<uint8> Bytes, FDateTime Utc);
	bool Pump(FDateTime& SavedUtc, FString& Error, bool bWait = false);
	static bool WriteSnapshot(const FString& Path, const TArray<uint8>& Bytes, const TArray<uint8>& Previous);
private:
	void Start(TArray<uint8> Bytes, FDateTime Utc);
	FString Path;
	TArray<uint8> LastGood, ActiveBytes, QueuedBytes;
	FDateTime ActiveUtc, QueuedUtc;
	TFuture<bool> Task;
	bool bRunning = false;
};
