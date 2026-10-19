#include "Estate/CozyEstateSaveIO.h"
#include "Async/Async.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

FCozyEstateSaveIO::~FCozyEstateSaveIO()
{
	FDateTime Utc; FString Error; Pump(Utc, Error, true);
}

void FCozyEstateSaveIO::Initialize(FString InPath, TArray<uint8> PreviousBytes)
{
	FDateTime Utc; FString Error; Pump(Utc, Error, true);
	Path = MoveTemp(InPath); LastGood = MoveTemp(PreviousBytes);
}

bool FCozyEstateSaveIO::WriteSnapshot(const FString& Path, const TArray<uint8>& Bytes, const TArray<uint8>& Previous)
{
	IFileManager& Files = IFileManager::Get();
	if (Bytes.IsEmpty() || !Files.MakeDirectory(*FPaths::GetPath(Path), true)) return false;
	const FString Pending = Path + TEXT(".pending");
	const FString Backup = FPaths::GetPath(Path) / (FPaths::GetBaseFilename(Path) + TEXT("_previous.sav"));
	if (!FFileHelper::SaveArrayToFile(Bytes, *Pending)) return false;
	// 마지막으로 검증한 바이트를 보관하므로 손상된 현재 파일로 정상본을 덮어쓰지 않는다.
	if (!Previous.IsEmpty() && !FFileHelper::SaveArrayToFile(Previous, *Backup)) return false;
	return Files.Move(*Path, *Pending, true, false, false, true);
}

void FCozyEstateSaveIO::Start(TArray<uint8> Bytes, FDateTime Utc)
{
	ActiveBytes = MoveTemp(Bytes); ActiveUtc = Utc; bRunning = true;
	Task = Async(EAsyncExecution::ThreadPool, [FilePath = Path, Snapshot = ActiveBytes, Previous = LastGood]()
	{
		return WriteSnapshot(FilePath, Snapshot, Previous);
	});
}

void FCozyEstateSaveIO::Enqueue(TArray<uint8> Bytes, FDateTime Utc)
{
	if (bRunning) { QueuedBytes = MoveTemp(Bytes); QueuedUtc = Utc; }
	else Start(MoveTemp(Bytes), Utc);
}

bool FCozyEstateSaveIO::Pump(FDateTime& SavedUtc, FString& Error, bool bWait)
{
	bool Wrote = false;
	while (bRunning && (bWait || Task.IsReady()))
	{
		if (Task.Get()) { LastGood = MoveTemp(ActiveBytes); SavedUtc = ActiveUtc; Wrote = true; }
		else Error = TEXT("저장 파일 쓰기에 실패했습니다. 직전 정상본은 유지됩니다.");
		bRunning = false;
		if (!QueuedBytes.IsEmpty()) Start(MoveTemp(QueuedBytes), QueuedUtc);
	}
	return Wrote;
}
