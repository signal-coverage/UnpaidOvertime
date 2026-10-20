// UNPAID: OVERTIME - first-person employee controlled by a human.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "UOCoreTypes.h"
#include "UOCharacter.generated.h"

class UCameraComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UInputAction;
class UInputMappingContext;
class UUOInteractionComponent;
class UUOFootstepComponent;
class AUOInteractableStation;
struct FInputActionValue;

/**
 *  Capsule + camera first-person character without a visible body. Enhanced Input actions and mappings are
 *  built at runtime in C++, so no input assets are required. Local presentation only lives here: subtle head bob,
 *  the focus outline on the aimed station and the overtime camera pulse.
 */
UCLASS()
class AUOCharacter : public ACharacter
{
	GENERATED_BODY()

public:

	AUOCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PawnClientRestart() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UUOInteractionComponent* GetInteraction() const { return Interaction; }
	UCameraComponent* GetCamera() const { return Camera; }

	/** Re-reads field of view (and other per-pawn options) from the saved settings */
	void ApplyUserSettings();

protected:

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UUOInteractionComponent> Interaction;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UUOFootstepComponent> Footsteps;

	/** Carried item held in the lower right of the view */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> HeldItemMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> HeldItemMaterial;

	/** Post-process outline for the focused station (built by Scripts/Editor/uo_materials.py) */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> OutlineMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> InteractAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> DropAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> PauseAction;

	/** F: the killer's sabotage / dismissal (does nothing for employees) */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SecondaryAction;

	/** M: mute / unmute the microphone (voice chat is M7; logged only in single player) */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MuteAction;

	EUOItemType ShownItem = EUOItemType::None;

	TWeakObjectPtr<AUOInteractableStation> HighlightedStation;

	float BobPhase = 0.f;
	/** 0..1: standing in a room whose lights were cut */
	float DarkBlend = 0.f;
	float BobWeight = 0.f;
	float BaseEyeZ = 64.f;

	/** Creates the input actions and the mapping context once */
	void EnsureInputAssets();

	void OnMove(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnJumpStarted();
	void OnJumpCompleted();
	void OnInteractStarted();
	void OnInteractCompleted();
	void OnDrop();
	void OnPause();
	void OnSecondaryStarted();
	void OnSecondaryCompleted();
	void OnMute();

	/** Outline post-process and user settings for the local player's pawn */
	void SetupLocalPresentation();

	void UpdateHeldItemVisual();
	void UpdateHeadBob(float DeltaSeconds);
	void UpdateFocusHighlight();
	void UpdateOvertimePostProcess();
};
