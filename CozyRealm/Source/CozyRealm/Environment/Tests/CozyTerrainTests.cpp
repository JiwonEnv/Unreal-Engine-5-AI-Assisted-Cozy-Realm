#include "Misc/AutomationTest.h"
#include "Environment/CozyPaintMask.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Estate/CozyEstateSaveIO.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyPaintInvariantTest, "CozyRealm.Terrain.PaintNormalizationUndoAndSave", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCozyPaintInvariantTest::RunTest(const FString& Parameters)
{
	FCozyPaintMask Mask;
	Mask.DomainId = FGuid::NewGuid(); Mask.Initialize(32);
	const TArray<FColor> Original = Mask.Pixels;
	FCozyPaintHistory History;
	History.Begin(Mask);
	for (int32 I = 0; I < 24; ++I) Mask.Stamp(FVector2D(.5, .5), FVector2D(.2, .3), I % 4, .4f);
	History.Commit(Mask);
	TestTrue(TEXT("중첩 도색 후 비율 합계 255"), Mask.IsValid());
	TestEqual(TEXT("영역 밖 픽셀 보존"), Mask.Pixels[0], Original[0]);
	const TArray<FColor> Painted = Mask.Pixels;
	TestTrue(TEXT("되돌리기"), History.Undo(Mask));
	TestTrue(TEXT("원래 픽셀 복구"), Mask.Pixels == Original);
	TestTrue(TEXT("다시 실행"), History.Redo(Mask));
	TestTrue(TEXT("도색 픽셀 복구"), Mask.Pixels == Painted);
	TestFalse(TEXT("잘못된 레이어 거부"), Mask.Stamp(FVector2D(.5), FVector2D(.2), 4, 1));
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	FObjectAndNameAsStringProxyArchive WriteArchive(Writer, false);
	FCozyPaintMask::StaticStruct()->SerializeItem(WriteArchive, &Mask, nullptr);
	FCozyPaintMask Loaded;
	FMemoryReader Reader(Bytes);
	FObjectAndNameAsStringProxyArchive ReadArchive(Reader, true);
	FCozyPaintMask::StaticStruct()->SerializeItem(ReadArchive, &Loaded, nullptr);
	TestTrue(TEXT("직렬화 복원"), Loaded.IsValid() && Loaded.DomainId == Mask.DomainId && Loaded.Pixels == Painted);
	TestTrue(TEXT("팔레트 버전과 재질 ID 복원"), Loaded.PaletteVersion == Mask.PaletteVersion && Loaded.MaterialIds == Mask.MaterialIds);
	Loaded.PaletteVersion = 99;
	TestFalse(TEXT("알 수 없는 팔레트는 적용 거부"), Loaded.IsValid());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozySaveQueueTest, "CozyRealm.Terrain.SaveQueueAndBackup", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCozySaveQueueTest::RunTest(const FString& Parameters)
{
	const FString Folder = FPaths::ProjectSavedDir() / TEXT("Automation/Terrain") / FGuid::NewGuid().ToString();
	const FString Path = Folder / TEXT("Slot.sav");
	const TArray<uint8> Original = {1, 2, 3}, First = {4, 5, 6}, Latest = {7, 8, 9};
	FCozyEstateSaveIO Writer;
	Writer.Initialize(Path, Original);
	const FDateTime Time = FDateTime::UtcNow();
	Writer.Enqueue(First, Time);
	Writer.Enqueue(Latest, Time + FTimespan::FromSeconds(1));
	FDateTime Saved; FString Error;
	TestTrue(TEXT("대기열 저장 완료"), Writer.Pump(Saved, Error, true));
	TestTrue(TEXT("저장 오류 없음"), Error.IsEmpty());
	TArray<uint8> Actual, Backup;
	TestTrue(TEXT("현재 파일 읽기"), FFileHelper::LoadFileToArray(Actual, *Path));
	TestTrue(TEXT("마지막 요청으로 저장"), Actual == Latest);
	TestTrue(TEXT("직전 정상본 읽기"), FFileHelper::LoadFileToArray(Backup, *(Folder / TEXT("Slot_previous.sav"))));
	TestTrue(TEXT("직전 정상본 유지"), Backup == First);
	TestFalse(TEXT("빈 저장 거부"), FCozyEstateSaveIO::WriteSnapshot(Path, {}, Latest));
	Actual.Reset(); FFileHelper::LoadFileToArray(Actual, *Path);
	TestTrue(TEXT("실패 시 기존 파일 유지"), Actual == Latest);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyPaintDomainTest, "CozyRealm.Terrain.PaintDomainIsolation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCozyPaintDomainTest::RunTest(const FString& Parameters)
{
	FCozyPaintMask Upper, Lower;
	Upper.DomainId = FGuid::NewGuid(); Lower.DomainId = FGuid::NewGuid();
	Upper.Initialize(32); Lower.Initialize(32);
	const TArray<FColor> LowerBefore = Lower.Pixels;
	FCozyPaintHistory History;
	History.Begin(Upper); Upper.Stamp(FVector2D(.5), FVector2D(.3), 1, 1); History.Commit(Upper);
	TestTrue(TEXT("하부 표면 불변"), Lower.Pixels == LowerBefore);
	TestFalse(TEXT("다른 도메인 되돌리기 거부"), History.Undo(Lower));
	TestTrue(TEXT("상부 되돌리기"), History.Undo(Upper));
	History.Begin(Upper); Upper.Stamp(FVector2D(.5), FVector2D(.3), 2, 1); History.Commit(Upper);
	TestFalse(TEXT("새 도색 후 과거 다시 실행 제거"), History.Redo(Upper));
	History.Begin(Upper); Upper.Stamp(FVector2D(.5), FVector2D(.3), 0, 1); History.Commit(Upper);
	TestTrue(TEXT("지우개 정규화 유지"), Upper.IsValid());
	FCozyPaintHistory Bounded;
	for (int32 I = 0; I < 200; ++I)
	{
		Bounded.Begin(Upper); Upper.Stamp(FVector2D(.5), FVector2D(.3), I % 3 + 1, 1); Bounded.Commit(Upper);
	}
	int32 UndoCount = 0;
	while (Bounded.Undo(Upper)) ++UndoCount;
	TestEqual(TEXT("200회 편집 후 이력 메모리 상한 16회"), UndoCount, 16);
	return true;
}
#endif
