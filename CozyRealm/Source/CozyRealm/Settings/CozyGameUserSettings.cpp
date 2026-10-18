#include "Settings/CozyGameUserSettings.h"
#include "Engine/Engine.h"

UCozyGameUserSettings* UCozyGameUserSettings::Get()
{
	return GEngine ? Cast<UCozyGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
}

void UCozyGameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();
	MasterVolume = 1.f;
	MusicVolume = 1.f;
	SfxVolume = 1.f;
	bCompactAlwaysOnTop = false;
}
