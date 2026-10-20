// UNPAID: OVERTIME - footsteps, jumps and landings.

#include "UOFootstepComponent.h"
#include "UOAudioSubsystem.h"
#include "UOAudioRules.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInterface.h"

namespace UOFootstepPrivate
{
	constexpr float TraceUp = 20.f;
	constexpr float TraceDown = 80.f;
	constexpr float JumpMinUpSpeed = 120.f;
	constexpr float LandMinAirTime = 0.3f;
	constexpr float SoftLandMinAirTime = 0.12f;
	/** After standing still the first step comes quickly */
	constexpr float RestartStrideFraction = 0.65f;
}

UUOFootstepComponent::UUOFootstepComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

FVector UUOFootstepComponent::GetFeetLocation() const
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
	}
	return Character->GetActorLocation() - FVector(0.f, 0.f, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
}

void UUOFootstepComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	using namespace UOFootstepPrivate;

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Movement)
	{
		return;
	}

	const bool bOnGround = Movement->IsMovingOnGround();
	const FVector Velocity = Character->GetVelocity();
	if (bFirstTick)
	{
		bFirstTick = false;
		bWasOnGround = bOnGround;
		return;
	}

	UUOAudioSubsystem* Audio = UUOAudioSubsystem::Get(this);
	if (bOnGround)
	{
		if (!bWasOnGround && Audio)
		{
			if (bJumpAndLandSounds && AirTime >= LandMinAirTime)
			{
				Audio->PlayAtLocation(UOSoundId::Land, GetFeetLocation(), VolumeMultiplier * FMath::Clamp(0.6f + AirTime * 0.6f, 0.6f, 1.f), FMath::FRandRange(0.95f, 1.05f));
				PlayStep(0.5f);
			}
			else if (AirTime >= SoftLandMinAirTime)
			{
				PlayStep(0.9f);
			}
		}
		AirTime = 0.f;

		const float Speed = Velocity.Size2D();
		const float Stride = FUOAudioRules::GetStrideLength(Speed);
		if (Speed >= MinSpeed)
		{
			DistanceSinceStep += Speed * DeltaTime;
			if (DistanceSinceStep >= Stride)
			{
				DistanceSinceStep = FMath::Fmod(DistanceSinceStep, Stride);
				PlayStep(1.f);
			}
		}
		else
		{
			DistanceSinceStep = Stride * RestartStrideFraction;
		}
	}
	else
	{
		if (bWasOnGround && bJumpAndLandSounds && Velocity.Z > JumpMinUpSpeed && Audio)
		{
			Audio->PlayAtLocation(UOSoundId::Jump, GetFeetLocation(), VolumeMultiplier, FMath::FRandRange(0.95f, 1.05f));
		}
		AirTime += DeltaTime;
	}
	bWasOnGround = bOnGround;
}

void UUOFootstepComponent::PlayStep(float Volume)
{
	using namespace UOFootstepPrivate;

	UUOAudioSubsystem* Audio = UUOAudioSubsystem::Get(this);
	UWorld* World = GetWorld();
	if (!Audio || !World)
	{
		return;
	}

	const FVector Feet = GetFeetLocation();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(UOFootstepTrace), false, GetOwner());
	FHitResult Hit;
	FString MaterialName;
	FVector Where = Feet;
	if (World->LineTraceSingleByChannel(Hit, Feet + FVector(0.f, 0.f, TraceUp), Feet - FVector(0.f, 0.f, TraceDown), ECC_Visibility, Params))
	{
		Where = Hit.ImpactPoint;
		if (const UPrimitiveComponent* Floor = Hit.GetComponent())
		{
			if (const UMaterialInterface* Material = Floor->GetMaterial(0))
			{
				MaterialName = Material->GetName();
			}
		}
	}

	const FName Sound = FUOAudioRules::GetFootstepSound(FUOAudioRules::GetSurfaceForMaterial(MaterialName));
	Audio->PlayAtLocation(Sound, Where, VolumeMultiplier * Volume * FMath::FRandRange(0.85f, 1.f), FMath::FRandRange(0.94f, 1.06f));
}
