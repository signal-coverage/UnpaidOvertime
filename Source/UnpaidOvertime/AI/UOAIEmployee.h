// UNPAID: OVERTIME - AI-controlled coworker pawn (mannequin body, name tag, carried item).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "UOCoreTypes.h"
#include "UOAIEmployee.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class USkeletalMesh;
class UUOInteractionComponent;
class UUOFootstepComponent;

/**
 *  A coworker body: the template mannequin (Manny or Quinn) driven by ABP_Unarmed from the movement
 *  component's velocity, tinted per employee, with the carried item in hand. Name tags are drawn by the HUD.
 *  Behavior lives in AUOAIController.
 */
UCLASS()
class AUOAIEmployee : public ACharacter
{
	GENERATED_BODY()

public:

	AUOAIEmployee();

	virtual void Tick(float DeltaSeconds) override;

	/** Body (Manny or Quinn) and outfit tint */
	void SetEmployeeLook(const FString& DisplayName, const FLinearColor& Color, bool bUseQuinn);

	UUOInteractionComponent* GetInteraction() const { return Interaction; }

protected:

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> HeldItemMesh;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UUOInteractionComponent> Interaction;

	/** Coworkers are audible: footsteps a bit softer than the player's */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UUOFootstepComponent> Footsteps;

	UPROPERTY()
	TObjectPtr<USkeletalMesh> MannyMesh;

	UPROPERTY()
	TObjectPtr<USkeletalMesh> QuinnMesh;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> BodyMaterials;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> HeldItemMaterial;

	EUOItemType ShownItem = EUOItemType::None;

	void UpdateHeldItem();
};
