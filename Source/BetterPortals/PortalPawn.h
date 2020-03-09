// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "HelperMacros.h"
#include "PortalPawn.generated.h"

/* Logging category for this class. */
DECLARE_LOG_CATEGORY_EXTERN(LogPortalPawn, Log, All);

/* Enum for holding movement state of the player. */
UENUM(BlueprintType)
enum class EMovementState : uint8
{
	RUNNING UMETA(DisplayName = "Running"),
	WALKING UMETA(DisplayName = "Walking"),
	CROUCHING UMETA(DisplayName = "Crouching"),
	FALLING UMETA(DisplayName = "Falling")
};

/* Movement structure to store any movement options for the portal character. */
USTRUCT(BlueprintType)
struct FCharacterSettings
{
	GENERATED_BODY()

public:

	/* Interaction trace distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Interaction")
		float interactionDistance;

	/* Force to throw something. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Interaction")
		float throwForce;

	/* Last interaction hit result. */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Interaction")
		FHitResult lastInteractHit;

	/* Last ground check hit result. */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Movement")
		FHitResult lastGroundHit;

	/* How fast to correct the players orientation after a teleport event. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Camera")
		float orientationCorrectionTime;

	/* Camera pitch amount */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Camera")
		float cameraPitch;

	/* Camera movement speed amount */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Camera")
		float mouseSpeed;

	/* The half height of the capsule when standing. NOTE: Will override the capsules half height on begin play. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movement")
		float standingHeight;

	/* The half height of the capsule when crouching. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movement")
		float crouchingHeight;

	/* Movement speed multiplier for adjusting. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movement")
		float movementSpeedMul;

	/* Movement dampening force for removing too much force and acceleration being added to the player. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movement")
		float movementDragMul;

	/* The up force multiplier to be applied while grounded and moving. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movement")
		float upForceMultiplier;

	/* The maximum up force multiplier to be applied while grounded and moving. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movement")
		float maxUpForce;

	/* Distance to look for the floor at the base of the characters capsule. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movement")
		float groundCheckDistance;

	/*  Movement speed for air movement split into X and Y movement for different forces for strafing. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movement")
		FVector2D airSpeed;

	/* Movement speed for walking. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movement")
		float walkSpeed;

	/* Movement speed for running. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movement")
		float runSpeed;

	/* Movement speed while crouched. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movement")
		float crouchSpeed;

	/* Time it takes to crouch down or up. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movement")
		float crouchTime;

	/* Amount of force multiplier to apply upwards along the capsules current up axis. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movement")
		float jumpForce;

	/* Characters mass. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movement")
		float mass;

	/* Can double jump? */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movement")
		bool doubleJump;

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

	/* Players grounded physics material.
	 * NOTE: will override physics material on capsule while grounded. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movement")
		class UPhysicalMaterial* physicsMaterialGrounded;

	/* Players air physics material.
	 * NOTE: will override physics material on capsule while in the air. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movement")
		class UPhysicalMaterial* physicsMaterialAir;

	bool crouching, uncrouching; // Track crouching state.

private:

	/* Code only variables for keeping track of current movement variables in use. */
	float currentMovementSpeed;
	EMovementState currentMovementState;

public:

	/* Default constructor. */
	FCharacterSettings()
	{
		// Setup default interaction values.
		interactionDistance = 300.0f;
		throwForce = 5.0f;

		// Camera default values.
		cameraPitch = 90.0f;
		mouseSpeed = 1.0f;

		// Setup default movement values.
		orientationCorrectionTime = 1.8f;
		standingHeight = 90.0f;
		crouchingHeight = 60.0f;
		movementSpeedMul = 6.0f;
		movementDragMul = 1.5f;
		upForceMultiplier = 0.8f;
		maxUpForce = 10.0f;
		airSpeed = FVector2D(60.0f, 100.0f);
		walkSpeed = 220.0f;
		runSpeed = 300.0f;
		crouchSpeed = 150.0f;
		crouchTime = 0.2f;
		jumpForce = 20.0f;
		mass = 50.0f;
		groundCheckDistance = 20.0f;
		currentMovementSpeed = walkSpeed;
		currentMovementState = EMovementState::WALKING; 
		doubleJump = false;
		physicsMaterialGrounded = nullptr;
		physicsMaterialAir = nullptr;
		crouching = false;
		uncrouching = false;
	}

	/* Check for any null values that could cause an error in the class. */
	bool IsValid()
	{
		return !physicsMaterialGrounded || !physicsMaterialAir;
	}

	/* Returns the current movement speed. */
	float GetCurrentMovementSpeed()
	{
		return currentMovementSpeed;
	}

	/* Returns the current movement state. */
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
		return !movementDir.IsZero();
	}

	/* Returns if the character is getting any movement input from the mouse. */
	bool IsInputtingMouseMovement()
	{
		return !mouseMovement.IsZero();
	}

	/* Returns if the pawn is currently grounded. */
	bool IsGrounded()
	{
		return lastGroundHit.bBlockingHit;
	}
};

/* Structure to hold crouching lerp information. */
USTRUCT(BlueprintType)
struct FCrouchLerp
{
	GENERATED_BODY()

public:

	FTimerHandle crouchTimerHandle;
	float timeToCrouch;
	float timeCrouchStarted;
	float startingHeight;
	float endingHeight;

public:

	/* Default Constructor. */
	FCrouchLerp(float time = 0.0f, float timeStarted = 0.0f) 
	{ 
		timeToCrouch = time;
		timeCrouchStarted = timeStarted;
	}
};

/* Character debug macro to enable and disable code.
 * NOTE: Define debug so it only runs when in editor. 
 * NOTE: Set CHAR_DEBUG to 1 bellow to include debug code in builds. */
#define CHAR_DEBUG WITH_EDITOR

/* Debug structure for enabling debug features within the class. 
 * NOTE: Removed using CHAR_DEBUG macro thats set to ENGINE_ONLY by default but can be changed above. */
USTRUCT(BlueprintType)
struct FCharacterDebugSettings
{
	GENERATED_BODY()

public:

	/* Print off information from the mouse input. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Interaction")
	bool debugMouseMovement;

	/* Show visual line trace view of interaction trace. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Interaction")
	bool debugInteractionTrace;

	/* Show visual line trace view of the ground check line trace looking for channel ECC_Pawn blocking.... */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Interaction")
	bool debugGroundTrace;

public:

	/* Default constructor. */
	FCharacterDebugSettings()
	{
		debugMouseMovement = false;
		debugInteractionTrace = false;
		debugGroundTrace = false;
	}
};

/* A character class to allow portal functionality while moving etc.
 * NOTE: Class will be based on physics based movement using sub-stepping. */
UCLASS()
class BETTERPORTALS_API APortalPawn : public APawn
{
	GENERATED_BODY()

public:
	
	/* Pointer to mesh. */
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "Movement")
	class USkeletalMeshComponent* playerMesh;

	/*  Pointer to capsule */
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "Movement")
	class UCapsuleComponent* playerCapsule;

	/* Game/Player Camera. */
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "Movement")
	class UCameraComponent* camera;

	/* Camera holding component to allow rotation while parent is simulating physics. */
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "Movement")
	class USceneComponent* cameraHolder;

	/* Physics handle component to hold physics object in-front of player. */
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "Movement")
	class UPhysicsHandleComponent* physicsHandle;

	/* Settings structures to manipulate the way the pawn moves etc. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movement")
	FCharacterSettings characterSettings;

	/* Settings for the debugging features in the pawn class. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movement")
	FCharacterDebugSettings debugSettings;

	// Last frames camera location in the world.
	FVector lastLocation;

private:

	int jumpCount; // Track number of times jumped while in air.
	FCrouchLerp crouchLerp; // Current crouch info.
	FVector originalRelativeLocation;
	FVector lastDirection;
	FRotator originalRelativeRotation;
	FRotator orientationAtStart; // Rotation of the capsule at the start of re-orientation.
	float orientationStart; // Start time of the orientation update func.
	bool orientation;

protected:
	
	/* Level start. */
	virtual void BeginPlay() override;

public:	

	/* Default constructor. */
	APortalPawn();

	/* Ran per TickGroup frame. */
	virtual void Tick(float DeltaTime) override;

	/* Called from PC to setup input. */
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

	/* Crouch lerp function. */
	UFUNCTION(Category = "Movement")
	void CrouchLerp();

	/* Interact action. */
	template<bool pressed> void InteractAction() { InteractAction(pressed); }
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void InteractAction(bool pressed);

	/* Release anything grabbed with the physics handle. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void ReleaseInteractable();

	/* Fire action. */
	template<bool pressed> void FireAction() { FireAction(pressed); }
	UFUNCTION(Category = "Movement")
	void FireAction(bool pressed);

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

	/* Perform ground check every frame to check if the player is actually grounded. */
	bool GroundCheck();

	/* Updates the pawns movement based on the current movement values. */
	void UpdateMovement(float deltaTime);

	/* Update the pawns rotation and camera pitch based on mouse movement. */
	void UpdateMouseMovement(float deltaTime);

	/* Updates the offset from the player that a held physics object should be at. Checks if this offset needs translating to the other side of a portal.
	 * Does work but not in use just decided to release the component if teleported through a portal. */
	void UpdatePhysicsHandleOffset();

	/* Ran from portal to a character when teleporting. Do any extra work in the player class after teleporting. */
	void PortalTeleport(class APortal* targetPortal);

	/* Timer function to return the player to the correct orientation after a teleport event from a portal class. */
	UFUNCTION(Category = "Movement")
	void ReturnToOrientation();

	/* An example function showing how to set up traces with portals with a recursion amount which is how many times it can go through a portal.
	 * Returns if it went through a portal during the trace. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	bool PortalTraceSingleExample(struct FHitResult& outHit, const FVector& start, const FVector& end, ECollisionChannel objectType, int maxPortalTrace);
};
