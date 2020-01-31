// Fill out your copyright notice in the Description page of Project Settings.

#include "PortalCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/InputComponent.h"
#include "Engine/EngineTypes.h"

APortalCharacter::APortalCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// Create sub-components.
	charMesh = CreateDefaultSubobject<USkeletalMeshComponent>("Char");
	charMesh->SetCollisionObjectType(ECC_Pawn);
	charMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Setup movement capsule around the player.
	charCapsule = CreateDefaultSubobject<UCapsuleComponent>("Char");
	charCapsule->SetCollisionObjectType(ECC_Pawn);
	charCapsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	charCapsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	charCapsule->SetCapsuleHalfHeight(90.0f);
	charCapsule->SetCapsuleRadius(40.0f);
	charCapsule->SetMassOverrideInKg(NAME_None, characterSettings.mass);
	charCapsule->SetSimulatePhysics(true);

	// Setup component order.
	RootComponent = charCapsule;
	charMesh->SetupAttachment(charCapsule);
}

void APortalCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

void APortalCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Check should update movement before updating.
	if (characterSettings.IsInputtingMovement())
	{
		UpdateMovement();
	}

	// Update movement velocity.
	movementSettings.linVelocity = charCapsule->GetPhysicsLinearVelocity();
	movementSettings.rotVelocity = charCapsule->GetPhysicsAngularVelocityInDegrees();
}

void APortalCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Setup action bindings.
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &APortalCharacter::JumpAction<true>);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &APortalCharacter::JumpAction<false>);
	PlayerInputComponent->BindAction("Run", IE_Pressed, this, &APortalCharacter::RunAction<true>);
	PlayerInputComponent->BindAction("Run", IE_Released, this, &APortalCharacter::RunAction<false>);
	PlayerInputComponent->BindAction("Crouch", IE_Pressed, this, &APortalCharacter::CrouchAction<true>);
	PlayerInputComponent->BindAction("Crouch", IE_Released, this, &APortalCharacter::CrouchAction<false>);
	PlayerInputComponent->BindAction("Interact", IE_Pressed, this, &APortalCharacter::InteractAction<true>);
	PlayerInputComponent->BindAction("Interact", IE_Released, this, &APortalCharacter::InteractAction<false>);

	// Setup axis bindings.
	PlayerInputComponent->BindAxis("MoveForward", this, &APortalCharacter::Forward);
	PlayerInputComponent->BindAxis("MoveRight", this, &APortalCharacter::Right);
	PlayerInputComponent->BindAxis("Turn", this, &APortalCharacter::Turn);
	PlayerInputComponent->BindAxis("LookUp", this, &APortalCharacter::LookUp);
}

void APortalCharacter::JumpAction(bool pressed)
{
	// Jump Action - Pressed
	if (pressed)
	{
		charMesh->AddForce(10000.0f * characterSettings.jumpForce * charCapsule->GetUpVector());
	}
	// Jump Action - Released
	else
	{
		// ...
	}
}

void APortalCharacter::RunAction(bool pressed)
{
	// If not crouching do the following.
	if (characterSettings.GetCurrentMovementState() != EMovementState::CROUCHING)
	{
		// Run Action - Pressed
		if (pressed)
		{
			characterSettings.SetMovement(EMovementState::RUNNING);
		}
		// Run Action - Released
		else
		{
			characterSettings.SetMovement(EMovementState::WALKING);
		}
	}
}

void APortalCharacter::CrouchAction(bool pressed)
{
	// Crouch Action - Pressed
	if (pressed)
	{
		characterSettings.SetMovement(EMovementState::CROUCHING);
	}
	// Crouch Action - Released
	else
	{
		characterSettings.SetMovement(EMovementState::WALKING);
	}
}

void APortalCharacter::InteractAction(bool pressed)
{
	// Interact Action - Pressed
	if (pressed)
	{
		// Perform interact line trace and fill relevant information in character settings.
		FHitResult interactHit;
		FVector startLocation = FVector::ZeroVector;
		FVector endLocation = startLocation + (0.0f * characterSettings.interactionDistance);
		FCollisionObjectQueryParams collObjParams;
		collObjParams.AddObjectTypesToQuery(UEngineTypes::ConvertToObjectType(ECC_Interactable));
		FCollisionQueryParams collParams;
		collParams.AddIgnoredActor(this);
		bool anything = GetWorld()->LineTraceSingleByObjectType(interactHit, startLocation, endLocation, collObjParams, collParams);
		characterSettings.lastInteractHit = interactHit;

		// If anything was hit run the interact function.
		if (anything)
		{
			Interact();
		}
	}
	// Interact Action - Released
	else
	{
		// ...
	}
}

void APortalCharacter::Forward(float val)
{
	characterSettings.movementDir.Y = val;
}

void APortalCharacter::Right(float val)
{
	characterSettings.movementDir.X = val;
}

void APortalCharacter::Turn(float val)
{
	characterSettings.mouseMovement.X = val;
}

void APortalCharacter::LookUp(float val)
{
	characterSettings.mouseMovement.Y = val;
}

void APortalCharacter::UpdateMovement()
{
	// Take care of character and camera rotation.



	// Take care of adding directional movement force to the player depending on mouse and keyboard movement input.




}

void APortalCharacter::Interact()
{

}