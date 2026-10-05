#include "Camera/CozyRealmCameraSettings.h"

#if WITH_EDITOR
void UCozyRealmCameraSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	OnSettingsChanged.Broadcast(this);
}
#endif
