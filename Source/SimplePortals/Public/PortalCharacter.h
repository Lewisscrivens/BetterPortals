// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "PortalCharacter.generated.h"

/* Macro definitions for collision channels. */
#define ECC_Interactable ECC_GameTraceChannel1
#define ECC_Portal ECC_GameTraceChannel2

/* Enum for holding movement state of the player. */
UENUM(BlueprintType)
enum class EMovementState : uint8
{
	RUNNING UMETA(DisplayName = "Running"),
	WALKING UMETA(DisplayName = "Walking"),
	CROUCHING UMETA(DisplayName = "Crouching")
}

/* Movement structure to store any movement options for the portal character. */
USTRUCT(BlueprintType)
struct FCharacterSettings
{
	GENERATED_BODY()

public:

	/* Interaction trace distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Interaction")
	float interactionDistance;

	/* Last interaction hit result. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Interaction")
	FHitResult lastInteractHit;

	/* Movement speed for walking. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movement")
	float walkSpeed;

	/* Movement speed for running. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movement")
	float runSpeed;

	/* Movement speed while crouched. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movement")
	float crouchSpeed;

	/* Amount of force multiplier to apply upwards along the capsules current up axis. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movement")
	float jumpForce;

	/* Characters mass. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movement")
	float mass;

	/* Current mouse input. NOTE: X - is turn. Y is lookUp. */
	UPROPERTY(BlueprintReadOnly, Category = "Movement")
	FVector2D mouseMovement;

	/* Current movement input. NOTE: X - Right. Y Forward. */
	UPROPERTY(BlueprintReadOnly, Category = "Movement")
	FVector2D movementDir;

	/* Current linear physics velocity. */
	UPROPERTY(BlueprintReadOnly, Category = "Movement")
	FVector linVelocity;

	/* Current angular physics velocity. */
	UPROPERTY(BlueprintReadOnly, Category = "Movement")
	FVector rotVelocity;

private:

	/* Code only variables for keeping track of current movement variables in use. */
	float currentMovementSpeed;
	EMovementState currentMovementState;

public:

	/* Default constructor. */
	FCharacterSettings()
	{
		// Setup default interaction values.
		interactionDistance = 100.0f;

		// Setup default movement values.
		walkSpeed = 600.0f;
		runSpeed = 1500.0f;
		crouchSpeed = 200.0f;
		jumpForce = 100.0f;
		mass = 50.0f;
		currentMovementSpeed = walkSpeed;
		currentMovementState = EMovementState::WALKING;
	}

	/* Returns the current movement speed. */
	UFUNCTION(BlueprintCallable, Category = "Movement")
	float GetCurrentMovementSpeed()
	{
		return currentMovementSpeed;
	}

	/* Returns the current movement state. */
	UFUNCTION(BlueprintCallable, Category = "Movement")
	EMovementState GetCurrentMovementState()
	{
		return currentMovementState;
	}

	/* Sets movement speed and state. */
	void SetMovement(EMovementState newState)
	{
		// Setup movement speed.
		switch (newState)
		{
		case EMovementState::RUNNING:
			currentMovementSpeed = runSpeed;
		break;
		case EMovementState::WALKING:
			currentMovementSpeed = walkSpeed;
		break;
		case EMovementState::CROUCHING:
			currentMovementSpeed = crouchSpeed;
		break;
		}

		// Setup new movement state.
		currentMovementState = newState;
	}

	/* Returns if the character is moving. */
	bool IsMoving()
	{
		if (!linVelocity.IsNearlyZero(0.5f) || !rotVelocity.IsNearlyZero(0.5f))
		{
			return true;
		}
		else return false;
	}

	/* Returns if the character is getting any movement input. */
	bool IsInputtingMovement()
	{
		if (!mouseMovement.IsZero() || !movementDir.IsZero())
		{
			return true;
		}
		else return false;
	}
};

/* A character class to allow portal functionality while moving etc.
 * NOTE: Class will be based on physics based movement using substepping. */
UCLASS()
class SIMPLEPORTALS_API APortalCharacter : public APawn
{
	GENERATED_BODY()

public:

	/* Pointer to mesh and capsule. */
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "Movement")
	class USkeletalMeshComponent* charMesh;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "Movement")
	class UCapsuleComponent* charCapsule;

	/* Settings structures to manipulate the way the pawn moves etc. */
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "Movement")
	FCharacterSettings characterSettings;

public:
	
	/* Constructor. */
	APortalCharacter();
	
	/* Level start. */
	virtual void BeginPlay() override;
	
	/* Frame tick. */
	virtual void Tick(float DeltaTime) override;

	/* Input initialization. */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/* ========== Movement input functions. ============== */

	/* Jump action. */
	template<bool pressed> void JumpAction() { JumpAction(pressed); }
	UFUNCTION(Category = "Movement")	
	void JumpAction(bool pressed);

	/* Run action. */
	template<bool pressed> void RunAction() { RunAction(pressed); }
	UFUNCTION(Category = "Movement")
	void RunAction(bool pressed);

	/* Crouch action. */
	template<bool pressed> void CrouchAction() { CrouchAction(pressed); }
	UFUNCTION(Category = "Movement")
	void CrouchAction(bool pressed);

	/* Interact action. */
	template<bool pressed> void InteractAction() { InteractAction(pressed); }
	UFUNCTION(Category = "Movement")
	void InteractAction(bool pressed);

	/* Move forward axis. */
	UFUNCTION(Category = "Movement")
	void Forward(float val);

	/* Move right axis. */
	UFUNCTION(Category = "Movement")
	void Right(float val);

	/* Rotational movement along the yaw axis with the mouse. */
	UFUNCTION(Category = "Movement")
	void Turn(float val);

	/* Rotational movement along the roll axis with the mouse. */
	UFUNCTION(Category = "Movement")
	void LookUp(float val);

	/* Updates the pawns movement based on the current movement values. */
	void UpdateMovement();

	/* Interact with whatever was hit, exit out and do nothing if its not possible to perform any functionality. */
	void Interact();
};
