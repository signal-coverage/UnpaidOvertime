// UNPAID: OVERTIME - first-person employee controlled by a human.

#include "UOCharacter.h"
#include "UOInteractionComponent.h"
#include "UOPlayerState.h"
#include "UOGameMode.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "UnpaidOvertime.h"

AUOCharacter::AUOCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(34.f, 88.f);
	BaseEyeHeight = 64.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->SetRelativeLocation(FVector(0.f, 0.f, BaseEyeHeight));
	Camera->bUsePawnControlRotation = true;
	Camera->SetFieldOfView(90.f);

	Interaction = CreateDefaultSubobject<UUOInteractionComponent>(TEXT("Interaction"));
	Interaction->bUseViewTrace = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	HeldItemMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeldItem"));
	HeldItemMesh->SetupAttachment(Camera);
	HeldItemMesh->SetStaticMesh(CubeFinder.Object);
	HeldItemMesh->SetMaterial(0, MaterialFinder.Object);
	HeldItemMesh->SetRelativeLocation(FVector(45.f, 20.f, -20.f));
	HeldItemMesh->SetRelativeRotation(FRotator(0.f, 20.f, 0.f));
	HeldItemMesh->SetRelativeScale3D(FVector(0.12f, 0.2f, 0.08f));
	HeldItemMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeldItemMesh->SetCastShadow(false);
	HeldItemMesh->SetOnlyOwnerSee(true);
	HeldItemMesh->SetVisibility(false);

	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->MaxWalkSpeed = 450.f;
	Movement->JumpZVelocity = 420.f;
	Movement->AirControl = 0.4f;
	Movement->BrakingDecelerationWalking = 2048.f;
}

void AUOCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateHeldItemVisual();
}

void AUOCharacter::EnsureInputAssets()
{
	if (MappingContext)
	{
		return;
	}

	MoveAction = NewObject<UInputAction>(this, TEXT("IA_UOMove"));
	MoveAction->ValueType = EInputActionValueType::Axis2D;

	LookAction = NewObject<UInputAction>(this, TEXT("IA_UOLook"));
	LookAction->ValueType = EInputActionValueType::Axis2D;

	JumpAction = NewObject<UInputAction>(this, TEXT("IA_UOJump"));
	JumpAction->ValueType = EInputActionValueType::Boolean;

	InteractAction = NewObject<UInputAction>(this, TEXT("IA_UOInteract"));
	InteractAction->ValueType = EInputActionValueType::Boolean;

	DropAction = NewObject<UInputAction>(this, TEXT("IA_UODrop"));
	DropAction->ValueType = EInputActionValueType::Boolean;

	RestartAction = NewObject<UInputAction>(this, TEXT("IA_UORestart"));
	RestartAction->ValueType = EInputActionValueType::Boolean;

	MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_UOEmployee"));

	// WASD -> 2D axis (X = right, Y = forward). Configure each mapping right away: MapKey may reallocate.
	{
		FEnhancedActionKeyMapping& Mapping = MappingContext->MapKey(MoveAction, EKeys::W);
		UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(MappingContext);
		Swizzle->Order = EInputAxisSwizzle::YXZ;
		Mapping.Modifiers.Add(Swizzle);
	}
	{
		FEnhancedActionKeyMapping& Mapping = MappingContext->MapKey(MoveAction, EKeys::S);
		UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(MappingContext);
		Swizzle->Order = EInputAxisSwizzle::YXZ;
		Mapping.Modifiers.Add(Swizzle);
		Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(MappingContext));
	}
	{
		FEnhancedActionKeyMapping& Mapping = MappingContext->MapKey(MoveAction, EKeys::A);
		Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(MappingContext));
	}
	MappingContext->MapKey(MoveAction, EKeys::D);

	// Mouse look: invert Y so moving the mouse forward looks up
	{
		FEnhancedActionKeyMapping& Mapping = MappingContext->MapKey(LookAction, EKeys::Mouse2D);
		UInputModifierNegate* NegateY = NewObject<UInputModifierNegate>(MappingContext);
		NegateY->bX = false;
		NegateY->bY = true;
		NegateY->bZ = false;
		Mapping.Modifiers.Add(NegateY);
	}

	MappingContext->MapKey(JumpAction, EKeys::SpaceBar);
	MappingContext->MapKey(InteractAction, EKeys::E);
	MappingContext->MapKey(DropAction, EKeys::Q);
	MappingContext->MapKey(RestartAction, EKeys::R);
}

void AUOCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	EnsureInputAssets();

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInput)
	{
		UE_LOG(LogUnpaidOvertime, Error, TEXT("AUOCharacter requires the Enhanced Input component (check DefaultInput.ini)."));
		return;
	}

	EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AUOCharacter::OnMove);
	EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &AUOCharacter::OnLook);
	EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &AUOCharacter::OnJumpStarted);
	EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &AUOCharacter::OnJumpCompleted);
	EnhancedInput->BindAction(InteractAction, ETriggerEvent::Started, this, &AUOCharacter::OnInteractStarted);
	EnhancedInput->BindAction(InteractAction, ETriggerEvent::Completed, this, &AUOCharacter::OnInteractCompleted);
	EnhancedInput->BindAction(InteractAction, ETriggerEvent::Canceled, this, &AUOCharacter::OnInteractCompleted);
	EnhancedInput->BindAction(DropAction, ETriggerEvent::Started, this, &AUOCharacter::OnDrop);
	EnhancedInput->BindAction(RestartAction, ETriggerEvent::Started, this, &AUOCharacter::OnRestart);
}

void AUOCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	EnsureInputAssets();

	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (PlayerController && PlayerController->GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->ClearAllMappings();
			Subsystem->AddMappingContext(MappingContext, 0);
		}
	}
}

void AUOCharacter::OnMove(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddMovementInput(GetActorForwardVector(), Axis.Y);
	AddMovementInput(GetActorRightVector(), Axis.X);
}

void AUOCharacter::OnLook(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}

void AUOCharacter::OnJumpStarted()
{
	Jump();
}

void AUOCharacter::OnJumpCompleted()
{
	StopJumping();
}

void AUOCharacter::OnInteractStarted()
{
	Interaction->BeginInteract();
}

void AUOCharacter::OnInteractCompleted()
{
	Interaction->EndInteract();
}

void AUOCharacter::OnDrop()
{
	Interaction->DropItem();
}

void AUOCharacter::OnRestart()
{
	if (AUOGameMode* GameMode = GetWorld()->GetAuthGameMode<AUOGameMode>())
	{
		GameMode->RequestNewDay(Cast<APlayerController>(GetController()));
	}
}

void AUOCharacter::UpdateHeldItemVisual()
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
		HeldItemMaterial->SetVectorParameterValue(TEXT("Color"), Color);
	}
	HeldItemMesh->SetRelativeScale3D(Carried == EUOItemType::InkCartridge ? FVector(0.12f, 0.2f, 0.08f) : FVector(0.22f, 0.3f, 0.02f));
}
