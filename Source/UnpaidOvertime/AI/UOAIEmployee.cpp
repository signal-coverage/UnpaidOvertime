// UNPAID: OVERTIME - AI-controlled coworker pawn.

#include "UOAIEmployee.h"
#include "UOAIController.h"
#include "UOInteractionComponent.h"
#include "UOFootstepComponent.h"
#include "UOPlayerState.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace UOAIEmployeePrivate
{
	const FName ColorParameter(TEXT("Color"));

	/** Mannequin material parameter (MI_Manny_* / MI_Quinn_*) */
	const FName PaintTintParameter(TEXT("Paint Tint"));

	const FName HandSocket(TEXT("hand_r"));

	constexpr float CapsuleRadius = 34.f;
	constexpr float CapsuleHalfHeight = 90.f;
}

AUOAIEmployee::AUOAIEmployee()
{
	using namespace UOAIEmployeePrivate;

	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(CapsuleRadius, CapsuleHalfHeight);

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MannyFinder(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> QuinnFinder(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple"));
	static ConstructorHelpers::FClassFinder<UAnimInstance> AnimFinder(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	MannyMesh = MannyFinder.Object;
	QuinnMesh = QuinnFinder.Object;

	// Mannequin feet on the capsule bottom, facing the capsule's forward
	USkeletalMeshComponent* Body = GetMesh();
	Body->SetSkeletalMesh(MannyMesh);
	Body->SetRelativeLocation(FVector(0.f, 0.f, -CapsuleHalfHeight));
	Body->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	Body->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	if (AnimFinder.Succeeded())
	{
		Body->SetAnimInstanceClass(AnimFinder.Class);
	}
	Body->SetCollisionProfileName(TEXT("CharacterMesh"));
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;

	HeldItemMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeldItem"));
	HeldItemMesh->SetupAttachment(Body, HandSocket);
	HeldItemMesh->SetStaticMesh(CubeFinder.Object);
	HeldItemMesh->SetMaterial(0, MaterialFinder.Object);
	HeldItemMesh->SetRelativeLocation(FVector(0.f, 8.f, 0.f));
	HeldItemMesh->SetRelativeScale3D(FVector(0.12f, 0.2f, 0.08f));
	HeldItemMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeldItemMesh->SetVisibility(false);

	Interaction = CreateDefaultSubobject<UUOInteractionComponent>(TEXT("Interaction"));
	Interaction->bUseViewTrace = false;

	Footsteps = CreateDefaultSubobject<UUOFootstepComponent>(TEXT("Footsteps"));
	Footsteps->VolumeMultiplier = 0.7f;
	Footsteps->bJumpAndLandSounds = false;

	AIControllerClass = AUOAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.f, 360.f, 0.f);
	Movement->MaxWalkSpeed = 240.f;
	Movement->BrakingDecelerationWalking = 900.f;
	Movement->bUseRVOAvoidance = true;
	Movement->AvoidanceConsiderationRadius = 250.f;
	Movement->NavAgentProps.AgentRadius = 40.f;
	Movement->NavAgentProps.AgentHeight = 180.f;
}

void AUOAIEmployee::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateHeldItem();
}

void AUOAIEmployee::SetEmployeeLook(const FString& DisplayName, const FLinearColor& Color, bool bUseQuinn)
{
	using namespace UOAIEmployeePrivate;

	USkeletalMeshComponent* Body = GetMesh();
	if (USkeletalMesh* Chosen = bUseQuinn ? QuinnMesh.Get() : MannyMesh.Get())
	{
		Body->SetSkeletalMesh(Chosen);
	}

	// Slot 0 = head and legs, slot 1 = torso: a dark neutral base and the employee's color as the "shirt"
	BodyMaterials.Reset();
	const int32 NumSlots = Body->GetNumMaterials();
	for (int32 Slot = 0; Slot < NumSlots; ++Slot)
	{
		if (UMaterialInstanceDynamic* Material = Body->CreateDynamicMaterialInstance(Slot))
		{
			const FLinearColor Tint = Slot == 1 ? Color : FMath::Lerp(Color, FLinearColor(0.08f, 0.08f, 0.09f), 0.78f);
			Material->SetVectorParameterValue(PaintTintParameter, Tint);
			BodyMaterials.Add(Material);
		}
	}
}

void AUOAIEmployee::UpdateHeldItem()
{
	const AUOPlayerState* Employee = GetPlayerState<AUOPlayerState>();
	const EUOItemType Carried = Employee ? Employee->GetCarriedItem() : EUOItemType::None;
	if (Carried == ShownItem)
	{
		return;
	}
	ShownItem = Carried;

	HeldItemMesh->SetVisibility(Carried != EUOItemType::None);
	if (Carried == EUOItemType::None)
	{
		return;
	}

	if (!HeldItemMaterial)
	{
		HeldItemMaterial = HeldItemMesh->CreateAndSetMaterialInstanceDynamic(0);
	}
	if (HeldItemMaterial)
	{
		const FLinearColor Color = Carried == EUOItemType::InkCartridge ? FLinearColor(0.02f, 0.02f, 0.03f) : FLinearColor(0.95f, 0.95f, 0.9f);
		HeldItemMaterial->SetVectorParameterValue(UOAIEmployeePrivate::ColorParameter, Color);
	}
	HeldItemMesh->SetRelativeScale3D(Carried == EUOItemType::InkCartridge ? FVector(0.1f, 0.16f, 0.07f) : FVector(0.22f, 0.3f, 0.015f));
}

