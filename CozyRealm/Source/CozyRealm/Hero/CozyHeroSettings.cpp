#include "Hero/CozyHeroSettings.h"

#if WITH_EDITOR
void UCozyHeroSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	OnSettingsChanged.Broadcast(this);
}
#endif
