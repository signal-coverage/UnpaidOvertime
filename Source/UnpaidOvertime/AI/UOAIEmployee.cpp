// UNPAID: OVERTIME - AI-controlled coworker pawn.

#include "UOAIEmployee.h"
#include "UOAIController.h"
#include "UOInteractionComponent.h"
#include "UOPlayerState.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace UOAIEmployeePrivate
{
	const FName ColorParameter(TEXT("Color"));

	UStaticMeshComponent* MakeMesh(AUOAIEmployee* Owner, const TCHAR* Name, UStaticMesh* Mesh, UMaterialInterface* Material, USceneComponent* Parent)
	{
		UStaticMeshComponent* Component = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Component->SetupAttachment(Parent);
		Component->SetStaticMesh(Mesh);
		Component->SetMaterial(0, Material);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return Component;
	}
}

AUOAIEmployee::AUOAIEmployee()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(34.f, 88.f);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	USceneComponent* Capsule = GetCapsuleComponent();

	BodyMesh = UOAIEmployeePrivate::MakeMesh(this, TEXT("Body"), CylinderFinder.Object, MaterialFinder.Object, Capsule);
	BodyMesh->SetRelativeLocation(FVector(0.f, 0.f, -23.f));
	BodyMesh->SetRelativeScale3D(FVector(0.6f, 0.6f, 1.3f));

	HeadMesh = UOAIEmployeePrivate::MakeMesh(this, TEXT("Head"), SphereFinder.Object, MaterialFinder.Object, Capsule);
	HeadMesh->SetRelativeLocation(FVector(0.f, 0.f, 62.f));
	HeadMesh->SetRelativeScale3D(FVector(0.4f));

	NoseMesh = UOAIEmployeePrivate::MakeMesh(this, TEXT("Nose"), CubeFinder.Object, MaterialFinder.Object, Capsule);
	NoseMesh->SetRelativeLocation(FVector(20.f, 0.f, 64.f));
	NoseMesh->SetRelativeScale3D(FVector(0.12f, 0.08f, 0.06f));

	HeldItemMesh = UOAIEmployeePrivate::MakeMesh(this, TEXT("HeldItem"), CubeFinder.Object, MaterialFinder.Object, Capsule);
	HeldItemMesh->SetRelativeLocation(FVector(40.f, 0.f, 5.f));
	HeldItemMesh->SetRelativeScale3D(FVector(0.2f, 0.3f, 0.1f));
	HeldItemMesh->SetVisibility(false);

	NameTag = CreateDefaultSubobject<UTextRenderComponent>(TEXT("NameTag"));
	NameTag->SetupAttachment(Capsule);
	NameTag->SetRelativeLocation(FVector(0.f, 0.f, 112.f));
	NameTag->SetHorizontalAlignment(EHTA_Center);
	NameTag->SetVerticalAlignment(EVRTA_TextCenter);
	NameTag->SetWorldSize(20.f);
	NameTag->SetTextRenderColor(FColor::White);
	NameTag->SetUsingAbsoluteRotation(true);
	NameTag->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Interaction = CreateDefaultSubobject<UUOInteractionComponent>(TEXT("Interaction"));
	Interaction->bUseViewTrace = false;

	AIControllerClass = AUOAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.f, 540.f, 0.f);
	Movement->MaxWalkSpeed = 300.f;
	Movement->BrakingDecelerationWalking = 1500.f;
}

void AUOAIEmployee::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateHeldItem();
	FaceNameTagToCamera();
}

void AUOAIEmployee::SetEmployeeLook(const FString& DisplayName, const FLinearColor& Color)
{
	NameTag->SetText(FText::FromString(DisplayName));
	NameTag->SetTextRenderColor(Color.ToFColor(true));

	if (!BodyMaterial)
	{
		BodyMaterial = BodyMesh->CreateAndSetMaterialInstanceDynamic(0);
		HeadMesh->SetMaterial(0, BodyMaterial);
	}
	if (BodyMaterial)
	{
		BodyMaterial->SetVectorParameterValue(UOAIEmployeePrivate::ColorParameter, Color);
	}

	if (UMaterialInstanceDynamic* NoseMaterial = NoseMesh->CreateAndSetMaterialInstanceDynamic(0))
	{
		NoseMaterial->SetVectorParameterValue(UOAIEmployeePrivate::ColorParameter, FLinearColor(0.05f, 0.05f, 0.05f));
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
		const FLinearColor Color = Carried == EUOItemType::InkCartridge ? FLinearColor(0.05f, 0.05f, 0.25f) : FLinearColor(0.95f, 0.95f, 0.9f);
		HeldItemMaterial->SetVectorParameterValue(UOAIEmployeePrivate::ColorParameter, Color);
	}
}

void AUOAIEmployee::FaceNameTagToCamera()
{
	const APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!CameraManager)
	{
		return;
	}

	const FVector ToCamera = CameraManager->GetCameraLocation() - NameTag->GetComponentLocation();
	if (!ToCamera.IsNearlyZero())
	{
		NameTag->SetWorldRotation(FRotator(0.f, ToCamera.Rotation().Yaw, 0.f));
	}
}
