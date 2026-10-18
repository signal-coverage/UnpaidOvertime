// UNPAID: OVERTIME - first-person employee controlled by a human.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "UOCoreTypes.h"
#include "UOCharacter.generated.h"

class UCameraComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UInputAction;
class UInputMappingContext;
class UUOInteractionComponent;
struct FInputActionValue;

/**
 *  Capsule + camera first-person character. Enhanced Input actions and mappings are built at runtime
 *  in C++, so no input assets are required.
 */
UCLASS()
class AUOCharacter : public ACharacter
{
	GENERATED_BODY()

public:

	AUOCharacter();

	virtual void Tick(float DeltaSeconds) override;
	virtual void PawnClientRestart() override;

	UUOInteractionComponent* GetInteraction() const { return Interaction; }
	UCameraComponent* GetCamera() const { return Camera; }

protected:

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UUOInteractionComponent> Interaction;

	/** Small box in the lower right of the view showing the carried item */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> HeldItemMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> HeldItemMaterial;

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
	TObjectPtr<UInputAction> RestartAction;

	EUOItemType ShownItem = EUOItemType::None;

	/** Creates the input actions and the mapping context once */
	void EnsureInputAssets();

	void OnMove(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnJumpStarted();
	void OnJumpCompleted();
	void OnInteractStarted();
	void OnInteractCompleted();
	void OnDrop();
	void OnRestart();

	void UpdateHeldItemVisual();
};
