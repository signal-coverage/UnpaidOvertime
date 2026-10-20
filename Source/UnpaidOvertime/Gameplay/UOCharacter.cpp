// UNPAID: OVERTIME - first-person employee controlled by a human.

#include "UOCharacter.h"
#include "UOInteractionComponent.h"
#include "UOFootstepComponent.h"
#include "UOInteractableStation.h"
#include "UOPlayerState.h"
#include "UOPlayerController.h"
#include "UOGameState.h"
#include "UOGameUserSettings.h"
#include "UOTerminal.h"
#include "UOEvidenceSubsystem.h"
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

namespace UOCharacterPrivate
{
	const TCHAR* OutlineMaterialPath = TEXT("/Game/UO/Materials/M_UO_Outline_PP.M_UO_Outline_PP");

	// Head bob: a couple of centimetres, only while walking on the ground
	constexpr float BobFrequency = 1.9f;		// steps per second at full walk speed
	constexpr float BobVertical = 1.1f;
	constexpr float BobLateral = 0.6f;

	// Overtime camera mood
	constexpr float OvertimeVignetteBase = 0.55f;
	constexpr float OvertimeVignettePulse = 0.25f;
	constexpr float OvertimePulseSpeed = 2.2f;
}

AUOCharacter::AUOCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(34.f, 88.f);
	BaseEyeHeight = 64.f;
	BaseEyeZ = BaseEyeHeight;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->SetRelativeLocation(FVector(0.f, 0.f, BaseEyeHeight));
	Camera->bUsePawnControlRotation = true;
	Camera->SetFieldOfView(90.f);

	Interaction = CreateDefaultSubobject<UUOInteractionComponent>(TEXT("Interaction"));
	Interaction->bUseViewTrace = true;

	Footsteps = CreateDefaultSubobject<UUOFootstepComponent>(TEXT("Footsteps"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	HeldItemMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeldItem"));
	HeldItemMesh->SetupAttachment(Camera);
	HeldItemMesh->SetStaticMesh(CubeFinder.Object);
	HeldItemMesh->SetMaterial(0, MaterialFinder.Object);
	HeldItemMesh->SetRelativeLocation(FVector(48.f, 17.f, -17.f));
	HeldItemMesh->SetRelativeRotation(FRotator(-8.f, 24.f, 6.f));
	HeldItemMesh->SetRelativeScale3D(FVector(0.12f, 0.2f, 0.08f));
	HeldItemMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeldItemMesh->SetCastShadow(false);
	HeldItemMesh->SetOnlyOwnerSee(true);
	HeldItemMesh->SetVisibility(false);

	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->MaxWalkSpeed = 420.f;
	Movement->JumpZVelocity = 420.f;
	Movement->AirControl = 0.4f;
	Movement->BrakingDecelerationWalking = 2048.f;
}

void AUOCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void AUOCharacter::SetupLocalPresentation()
{
	// Runs on possession by a local player (BeginPlay happens before possession, too early to know)
	if (!OutlineMaterial)
	{
		OutlineMaterial = LoadObject<UMaterialInterface>(nullptr, UOCharacterPrivate::OutlineMaterialPath);
		if (OutlineMaterial)
		{
			Camera->PostProcessSettings.AddBlendable(OutlineMaterial, 1.f);
		}
		else
		{
			UE_LOG(LogUnpaidOvertime, Warning, TEXT("Focus outline material %s not found; stations will not be outlined."), UOCharacterPrivate::OutlineMaterialPath);
		}
	}
	ApplyUserSettings();
}

void AUOCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AUOInteractableStation* Station = HighlightedStation.Get())
	{
		Station->SetHighlighted(false);
	}
	Super::EndPlay(EndPlayReason);
}

void AUOCharacter::ApplyUserSettings()
{
	Camera->SetFieldOfView(UUOGameUserSettings::Get()->GetFieldOfView());
}

void AUOCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateHeldItemVisual();

	if (IsLocallyControlled())
	{
		UpdateHeadBob(DeltaSeconds);
		UpdateFocusHighlight();
		UpdateOvertimePostProcess();
	}
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

	PauseAction = NewObject<UInputAction>(this, TEXT("IA_UOPause"));
	PauseAction->ValueType = EInputActionValueType::Boolean;

	SecondaryAction = NewObject<UInputAction>(this, TEXT("IA_UOSecondary"));
	SecondaryAction->ValueType = EInputActionValueType::Boolean;

	MuteAction = NewObject<UInputAction>(this, TEXT("IA_UOMute"));
	MuteAction->ValueType = EInputActionValueType::Boolean;

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

	// Raw mouse delta; sensitivity and inversion come from the user settings in OnLook
	MappingContext->MapKey(LookAction, EKeys::Mouse2D);

	MappingContext->MapKey(JumpAction, EKeys::SpaceBar);
	MappingContext->MapKey(InteractAction, EKeys::E);
	MappingContext->MapKey(DropAction, EKeys::Q);
	MappingContext->MapKey(SecondaryAction, EKeys::F);
	MappingContext->MapKey(MuteAction, EKeys::M);
	MappingContext->MapKey(PauseAction, EKeys::Escape);
	MappingContext->MapKey(PauseAction, EKeys::P);
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
	EnhancedInput->BindAction(PauseAction, ETriggerEvent::Started, this, &AUOCharacter::OnPause);
	EnhancedInput->BindAction(SecondaryAction, ETriggerEvent::Started, this, &AUOCharacter::OnSecondaryStarted);
	EnhancedInput->BindAction(SecondaryAction, ETriggerEvent::Completed, this, &AUOCharacter::OnSecondaryCompleted);
	EnhancedInput->BindAction(SecondaryAction, ETriggerEvent::Canceled, this, &AUOCharacter::OnSecondaryCompleted);
	EnhancedInput->BindAction(MuteAction, ETriggerEvent::Started, this, &AUOCharacter::OnMute);
}

void AUOCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	EnsureInputAssets();
	SetupLocalPresentation();

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
	const UUOGameUserSettings* Settings = UUOGameUserSettings::Get();
	const FVector2D Axis = Value.Get<FVector2D>() * Settings->GetMouseSensitivity();

	// Moving the mouse forward looks up unless inverted
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Settings->IsInvertY() ? Axis.Y : -Axis.Y);
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
	// Terminals open their log on screen (local UI); everything else goes through the task manager
	AUOTerminal* Terminal = Cast<AUOTerminal>(Interaction->GetFocusedStation());
	if (Terminal && Interaction->GetCurrentOption().Kind == EUOInteractionKind::ReadTerminal)
	{
		if (AUOPlayerController* PC = Cast<AUOPlayerController>(GetController()))
		{
			PC->OpenTerminal(Terminal);
		}
		return;
	}
	Interaction->BeginInteract();
}

void AUOCharacter::OnSecondaryStarted()
{
	Interaction->BeginSecondary();
}

void AUOCharacter::OnSecondaryCompleted()
{
	Interaction->EndSecondary();
}

void AUOCharacter::OnMute()
{
	if (AUOPlayerController* PC = Cast<AUOPlayerController>(GetController()))
	{
		PC->ToggleMicMute();
	}
}

void AUOCharacter::OnInteractCompleted()
{
	Interaction->EndInteract();
}

void AUOCharacter::OnDrop()
{
	Interaction->DropItem();
}

void AUOCharacter::OnPause()
{
	if (AUOPlayerController* PC = Cast<AUOPlayerController>(GetController()))
	{
		PC->OpenPauseMenu();
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
		const FLinearColor Color = Carried == EUOItemType::InkCartridge ? FLinearColor(0.02f, 0.02f, 0.03f) : FLinearColor(0.95f, 0.95f, 0.9f);
		HeldItemMaterial->SetVectorParameterValue(TEXT("Color"), Color);
	}
	HeldItemMesh->SetRelativeScale3D(Carried == EUOItemType::InkCartridge ? FVector(0.03f, 0.07f, 0.035f) : FVector(0.15f, 0.11f, 0.004f));
}

void AUOCharacter::UpdateHeadBob(float DeltaSeconds)
{
	using namespace UOCharacterPrivate;

	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	const float Speed = GetVelocity().Size2D();
	const float SpeedAlpha = Movement && Movement->IsMovingOnGround() ? FMath::Clamp(Speed / FMath::Max(1.f, Movement->MaxWalkSpeed), 0.f, 1.f) : 0.f;

	BobWeight = FMath::FInterpTo(BobWeight, SpeedAlpha, DeltaSeconds, 8.f);
	BobPhase += DeltaSeconds * BobFrequency * 2.f * PI * FMath::Max(0.35f, SpeedAlpha);
	if (BobPhase > 2.f * PI * 100.f)
	{
		BobPhase -= 2.f * PI * 100.f;
	}

	// Two vertical dips per stride cycle, one lateral sway
	const float Vertical = -FMath::Abs(FMath::Sin(BobPhase)) * BobVertical * BobWeight;
	const float Lateral = FMath::Sin(BobPhase * 0.5f) * BobLateral * BobWeight;
	Camera->SetRelativeLocation(FVector(0.f, Lateral, BaseEyeZ + Vertical + BobVertical * 0.5f * BobWeight));
}

void AUOCharacter::UpdateFocusHighlight()
{
	AUOInteractableStation* Focused = Interaction->GetFocusedStation();
	if (Focused == HighlightedStation.Get())
	{
		return;
	}

	if (AUOInteractableStation* Previous = HighlightedStation.Get())
	{
		Previous->SetHighlighted(false);
	}
	HighlightedStation = Focused;
	if (Focused)
	{
		Focused->SetHighlighted(true);
	}
}

void AUOCharacter::UpdateOvertimePostProcess()
{
	using namespace UOCharacterPrivate;

	const AUOGameState* GameState = GetWorld()->GetGameState<AUOGameState>();
	const bool bOvertime = GameState && GameState->GetPhase() == EUODayPhase::Overtime;

	FPostProcessSettings& Settings = Camera->PostProcessSettings;

	// A room whose camera and lights were cut is a dark, blind zone (the windows still let some daylight in,
	// so the view itself is pushed down while you stand in it)
	const UUOEvidenceSubsystem* Evidence = UUOEvidenceSubsystem::Get(this);
	const bool bDark = Evidence && Evidence->IsRoomDark(Evidence->GetRoomOf(this));
	DarkBlend = FMath::FInterpConstantTo(DarkBlend, bDark ? 1.f : 0.f, GetWorld()->GetDeltaSeconds(), 1.5f);
	Settings.bOverride_AutoExposureBias = DarkBlend > 0.001f;
	Settings.AutoExposureBias = -3.2f * DarkBlend;
	if (DarkBlend > 0.001f)
	{
		Settings.bOverride_VignetteIntensity = true;
		Settings.VignetteIntensity = 0.9f * DarkBlend;
		if (!bOvertime)
		{
			Settings.bOverride_SceneColorTint = true;
			Settings.SceneColorTint = FLinearColor(0.75f, 0.8f, 1.f);
			return;
		}
	}

	if (!bOvertime)
	{
		Settings.bOverride_VignetteIntensity = false;
		Settings.bOverride_SceneColorTint = false;
		return;
	}

	const float Pulse = 0.5f + 0.5f * FMath::Sin(GetWorld()->GetTimeSeconds() * OvertimePulseSpeed);
	Settings.bOverride_VignetteIntensity = true;
	Settings.VignetteIntensity = OvertimeVignetteBase + OvertimeVignettePulse * Pulse;
	Settings.bOverride_SceneColorTint = true;
	Settings.SceneColorTint = FLinearColor(1.f, 0.86f - 0.06f * Pulse, 0.84f - 0.08f * Pulse);
}
