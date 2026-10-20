// UNPAID: OVERTIME - a dismissed employee (optional kill, doc 6.4): stylized collapsed mannequin + a red termination letter.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UOBodyActor.generated.h"

class USkeletalMeshComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class USkeletalMesh;

/**
 *  Family-friendly gore: no blood, just the mannequin lying flat on its back on a clear patch of floor (posed, then
 *  relaxed by the ragdoll when the mesh has a physics asset) and a red termination letter beside it. Removed overnight.
 */
UCLASS()
class AUOBodyActor : public AActor
{
	GENERATED_BODY()

public:

	AUOBodyActor();

	/** Mesh and outfit of the dismissed employee */
	void SetupBody(USkeletalMesh* Mesh, const FLinearColor& Color, const FString& Name);

protected:

	UPROPERTY(VisibleAnywhere, Category="Body")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category="Body")
	TObjectPtr<USkeletalMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category="Body")
	TObjectPtr<USceneComponent> LetterRoot;

	UPROPERTY(VisibleAnywhere, Category="Body")
	TObjectPtr<UStaticMeshComponent> Letter;

	UPROPERTY(VisibleAnywhere, Category="Body")
	TObjectPtr<UStaticMeshComponent> LetterStripe;

	UPROPERTY(VisibleAnywhere, Category="Body")
	TObjectPtr<UTextRenderComponent> LetterText;
};
