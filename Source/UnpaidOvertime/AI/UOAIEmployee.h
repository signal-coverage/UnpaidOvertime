// UNPAID: OVERTIME - AI-controlled coworker pawn (greybox body, name tag, carried item).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "UOCoreTypes.h"
#include "UOAIEmployee.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class UMaterialInstanceDynamic;
class UUOInteractionComponent;

/**
 *  A coworker body: colored cylinder with a head, a "nose" showing where it faces,
 *  a floating name tag and a box when carrying an item. Behavior lives in AUOAIController.
 */
UCLASS()
class AUOAIEmployee : public ACharacter
{
	GENERATED_BODY()

public:

	AUOAIEmployee();

	virtual void Tick(float DeltaSeconds) override;

	/** Applies name tag text and body color */
	void SetEmployeeLook(const FString& DisplayName, const FLinearColor& Color);

	UUOInteractionComponent* GetInteraction() const { return Interaction; }

protected:

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> HeadMesh;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> NoseMesh;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> HeldItemMesh;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UTextRenderComponent> NameTag;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UUOInteractionComponent> Interaction;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BodyMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> HeldItemMaterial;

	EUOItemType ShownItem = EUOItemType::None;

	void UpdateHeldItem();
	void FaceNameTagToCamera();
};
