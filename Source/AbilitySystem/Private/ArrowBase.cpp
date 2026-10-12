#include "ArrowBase.h"

#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"

AArrowBase::AArrowBase()
{
	KillCam = CreateDefaultSubobject<UCameraComponent>(TEXT("KillCam"));
	KillCam->SetupAttachment(HitBox);
}
