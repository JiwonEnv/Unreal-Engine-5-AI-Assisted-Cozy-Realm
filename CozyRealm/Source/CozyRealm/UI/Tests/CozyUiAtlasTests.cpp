#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Texture2D.h"
#include "UI/Kit/CozyUiTheme.h"
#include "UI/Kit/CozyUiWidgets.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyUiAtlasRegionsTest, "CozyRealm.UI.Atlas.SharedRegions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCozyUiAtlasRegionsTest::RunTest(const FString& Parameters)
{
	UCozyUiTheme* Theme = NewObject<UCozyUiTheme>();
	UTexture2D* Texture = UTexture2D::CreateTransient(512, 256);
	FCozyUiAtlasImage First;
	First.Texture = Texture;
	First.Origin = FVector2D(128, 0);
	First.Size = FVector2D(128, 128);
	Theme->AtlasImages.Add(TEXT("Icon.First"), First);
	FCozyUiAtlasImage Second = First;
	Second.Origin = FVector2D(256, 128);
	Second.Size = FVector2D(64, 128);
	Theme->AtlasImages.Add(TEXT("Icon.Second"), Second);

	const FSlateBrush FirstBrush = *Theme->FindImage(TEXT("icon.first"));
	const FSlateBrush SecondBrush = *Theme->FindImage(TEXT("Icon.Second"));
	TestTrue(TEXT("다른 아이콘은 같은 텍스처를 공유한다"), FirstBrush.GetResourceObject() == SecondBrush.GetResourceObject());
	const FBox2f Region = FirstBrush.GetUVRegion();
	TestTrue(TEXT("첫 번째 칸의 시작 좌표"), Region.Min.Equals(FVector2f(.25f, 0.f)));
	TestTrue(TEXT("첫 번째 칸의 끝 좌표"), Region.Max.Equals(FVector2f(.5f, .5f)));
	TestTrue(TEXT("비율 맞춤은 전체 시트가 아닌 아이콘 칸을 기준으로 한다"),
		CozyUiFit::Resolve(SecondBrush, FVector2D(64,64), ECozyUiImageFit::KeepRatio).Equals(FVector2D(32,64)));
	TestTrue(TEXT("원래 크기는 아이콘 칸 크기다"),
		CozyUiFit::Resolve(SecondBrush, FVector2D(64,64), ECozyUiImageFit::Original).Equals(FVector2D(64,128)));

	FSlateBrush Legacy;
	Legacy.ImageSize = FVector2D(300,150);
	Theme->Images.Add(TEXT("Window"), Legacy);
	TestTrue(TEXT("기존 배경 브러시를 보존한다"), Theme->FindImage(TEXT("Window"))->ImageSize == Legacy.ImageSize);
	First.Origin = FVector2D(500,0);
	Theme->AtlasImages.Add(TEXT("Window"), First);
	TestTrue(TEXT("시트 범위를 벗어나면 기존 이미지로 돌아간다"), Theme->FindImage(TEXT("Window"))->ImageSize == Legacy.ImageSize);
	Theme->AtlasImages.Add(TEXT("Invalid"), First);
	TestNull(TEXT("잘못된 영역과 대체 이미지가 없으면 표시하지 않는다"), Theme->FindImage(TEXT("Invalid")));
	TestNull(TEXT("빈 이름은 표시하지 않는다"), Theme->FindImage(NAME_None));
	TestTrue(TEXT("공유 시트 이름도 편집기 선택 목록에 포함한다"), UCozyUiTheme::GetImageNameOptions().Contains(TEXT("Icon.First")));
	Second.Texture = UTexture2D::CreateTransient(32,32);
	Second.TextureSize = FVector2D(512,256);
	Theme->AtlasImages.Add(TEXT("Scaled"), Second);
	const FSlateBrush* Scaled = Theme->FindImage(TEXT("Scaled"));
	if (TestNotNull(TEXT("임시·축소 텍스처 크기에서도 원본 칸을 찾는다"), Scaled))
	{
		const FBox2f ScaledUV = Scaled->GetUVRegion();
		TestTrue(TEXT("UV는 시트 원본 크기를 기준으로 한다"), ScaledUV.Min.Equals(FVector2f(.5f,.5f)));
		TestTrue(TEXT("축소된 텍스처에서도 아이콘 원래 크기를 유지한다"),
			CozyUiFit::Resolve(*Scaled,FVector2D(64,64),ECozyUiImageFit::Original).Equals(FVector2D(64,128)));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCozyUiAtlasActiveThemeTest, "CozyRealm.UI.Atlas.ActiveTheme",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCozyUiAtlasActiveThemeTest::RunTest(const FString& Parameters)
{
	const UCozyUiTheme* Theme = UCozyUiSettings::LoadDefaultTheme();
	if (!TestNotNull(TEXT("현재 기본 테마를 불러온다"), Theme)) { return false; }
	TestEqual(TEXT("아이콘 이름 35개를 이전한다"), Theme->AtlasImages.Num(), 35);
	TSet<const UObject*> SharedTextures;
	for (const auto& Pair : Theme->AtlasImages)
	{
		const FSlateBrush* Brush = Theme->FindImage(Pair.Key);
		if (!TestNotNull(*Pair.Key.ToString(), Brush)) { continue; }
		TestTrue(TEXT("공유 시트를 참조한다"), Brush->GetResourceObject() == Pair.Value.Texture.Get());
		TestTrue(TEXT("아이콘 칸 크기를 보존한다"), FVector2D(Brush->ImageSize).Equals(Pair.Value.Size));
		const FBox2f UV = Brush->GetUVRegion();
		TestTrue(TEXT("표시 영역은 공유 시트 안에 있다"), UV.bIsValid && UV.Min.X >= 0.f && UV.Min.Y >= 0.f && UV.Max.X <= 1.f && UV.Max.Y <= 1.f);
		SharedTextures.Add(Brush->GetResourceObject());
	}
	TestEqual(TEXT("아이콘 텍스처는 3장이다"), SharedTextures.Num(), 3);
	// 기존 배경 이름 5개는 그대로 (개수가 아니라 이름으로 · 공유 시트 밖 아이콘이 더 붙을 수 있음 · 예: 작은 창 핀 Icon.PinTop/PinTopOff 2026-10-22)
	for (const TCHAR* Name : { TEXT("Chip"), TEXT("Header"), TEXT("Highlight"), TEXT("Panel"), TEXT("Window") })
	{
		TestTrue(FString::Printf(TEXT("기존 배경 이름 유지: %s"), Name), Theme->Images.Contains(FName(Name)));
	}
	TestTrue(TEXT("시설 메뉴 줄별 선택 이름을 유지한다"), UCozyUiTheme::GetImageNameOptions().Contains(TEXT("Menu.@Row")));
	return true;
}
#endif
