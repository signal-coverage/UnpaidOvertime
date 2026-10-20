// UNPAID: OVERTIME - a dismissed employee.

#include "UOBodyActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "UOBodyActor"

namespace UOBodyPrivate
{
	const FName PaintTintParameter(TEXT("Paint Tint"));
	const FName ColorParameter(TEXT("Color"));
}

AUOBodyActor::AUOBodyActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	Body->SetCollisionProfileName(TEXT("Ragdoll"));
	Body->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	// A sheet of paper on the floor (30 x 21 cm, scaled up a bit so it reads from standing height) with a red header
	LetterRoot = CreateDefaultSubobject<USceneComponent>(TEXT("LetterRoot"));
	LetterRoot->SetupAttachment(Root);
	// Beside the torso, on the floor
	LetterRoot->SetRelativeLocation(FVector(10.f, 78.f, 0.5f));
	LetterRoot->SetRelativeRotation(FRotator(0.f, 18.f, 0.f));
	LetterRoot->SetRelativeScale3D(FVector(1.6f));

	Letter = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Letter"));
	Letter->SetupAttachment(LetterRoot);
	Letter->SetStaticMesh(CubeFinder.Object);
	Letter->SetMaterial(0, MaterialFinder.Object);
	Letter->SetRelativeLocation(FVector(0.f, 0.f, 0.2f));
	Letter->SetRelativeScale3D(FVector(0.30f, 0.21f, 0.004f));
	Letter->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Letter->SetCanEverAffectNavigation(false);

	LetterStripe = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LetterStripe"));
	LetterStripe->SetupAttachment(LetterRoot);
	LetterStripe->SetStaticMesh(CubeFinder.Object);
	LetterStripe->SetMaterial(0, MaterialFinder.Object);
	LetterStripe->SetRelativeLocation(FVector(-12.f, 0.f, 0.45f));
	LetterStripe->SetRelativeScale3D(FVector(0.045f, 0.21f, 0.004f));
	LetterStripe->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	LetterText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("LetterText"));
	LetterText->SetupAttachment(LetterRoot);
	LetterText->SetRelativeLocation(FVector(0.f, 0.f, 0.7f));
	LetterText->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
	LetterText->SetWorldSize(1.9f);
	LetterText->SetHorizontalAlignment(EHTA_Center);
	LetterText->SetVerticalAlignment(EVRTA_TextCenter);
	LetterText->SetTextRenderColor(FColor(150, 10, 20));
	LetterText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AUOBodyActor::SetupBody(USkeletalMesh* Mesh, const FLinearColor& Color, const FString& Name)
{
	using namespace UOBodyPrivate;

	if (Mesh)
	{
		Body->SetSkeletalMesh(Mesh);
		for (int32 Slot = 0; Slot < Body->GetNumMaterials(); ++Slot)
		{
			if (UMaterialInstanceDynamic* Material = Body->CreateDynamicMaterialInstance(Slot))
			{
				const FLinearColor Tint = Slot == 1 ? Color : FMath::Lerp(Color, FLinearColor(0.08f, 0.08f, 0.09f), 0.78f);
				// Drained of color: they are not coming back to work
				Material->SetVectorParameterValue(PaintTintParameter, FMath::Lerp(Tint, FLinearColor(0.35f, 0.35f, 0.37f), 0.45f));
			}
		}
	}

	// Lying flat on its back along the actor's forward axis, centred on the actor (the game mode picked a clear
	// patch of floor): mesh up (+Z, feet to head) -> actor forward, mesh front (+Y) -> up
	const FRotator Lying = FRotationMatrix::MakeFromXZ(FVector(0.f, 1.f, 0.f), FVector(1.f, 0.f, 0.f)).Rotator();
	Body->SetRelativeRotation(Lying);
	Body->SetRelativeLocation(FVector(-90.f, 0.f, 16.f));
	if (Body->GetPhysicsAsset())
	{
		// Let the limbs settle on the floor (no impulse: it starts flat, so it stays flat)
		Body->SetAllBodiesSimulatePhysics(true);
		Body->SetSimulatePhysics(true);
		Body->WakeAllRigidBodies();
	}
	if (UMaterialInstanceDynamic* Paper = Letter->CreateAndSetMaterialInstanceDynamic(0))
	{
		Paper->SetVectorParameterValue(ColorParameter, FLinearColor(0.95f, 0.93f, 0.88f));
	}
	if (UMaterialInstanceDynamic* Stripe = LetterStripe->CreateAndSetMaterialInstanceDynamic(0))
	{
		Stripe->SetVectorParameterValue(ColorParameter, FLinearColor(0.7f, 0.02f, 0.04f));
	}
	LetterText->SetText(FText::Format(LOCTEXT("Letter", "NOTICE OF TERMINATION\n{0}\nEffective immediately."), FText::FromString(Name)));
}

#undef LOCTEXT_NAMESPACE
