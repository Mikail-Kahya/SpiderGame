// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "BaseSpider.generated.h"

class USpringArmComponent;
class UCapsuleComponent;
class UArrowComponent;
class UCameraComponent;

UCLASS()
class SPIDERGAME_API ABaseSpider : public APawn
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	ABaseSpider();

	// Root
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category="Default")
	TObjectPtr<UCapsuleComponent> Collider;

	// Body nesting
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category="Default")
	TObjectPtr<USceneComponent> Spider;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category="Default")
	TObjectPtr<USkeletalMeshComponent> SkeletalMesh;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category="Default")
	TObjectPtr<UArrowComponent> Arrow;
	
	// Camera nesting
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category="Default")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category="Default")
	TObjectPtr<USpringArmComponent> Arm;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category="Default")
	TObjectPtr<UArrowComponent> WebSocket;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category="Sound")
	TObjectPtr<UAudioComponent> WalkSound;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category="Sound")
	TObjectPtr<UAudioComponent> WebWalkSound;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category="Sound")
	TObjectPtr<UAudioComponent> LandingSound;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement")
	float MovementSpeed{ 1000.0f };
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement")
	float MaxCameraOffset{ 50.0f };
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement")
	float WallInterpolateTime{ 0.7f };
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement")
	float LargeLerpCompensation{ 2.f };
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement")
	float JumpForwardScale{ 0.5f };
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Physics")
	float Gravity{ 2000.0f };
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Physics")
	float Drag{ 0.93f };
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Collision")
	float WallCheckDistance{ 100.0f };
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Collision")
	float GroundCheckDistance{ 50.f };
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Collision")
	FName WebTag{ "Web" };
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Collision")
	FName WebLineTag{ "WebLine" };
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Collision")
	float JumpImmuneTime{ 0.1f };
	
	UFUNCTION(BlueprintCallable)
	bool IsGrounded() const;
	UFUNCTION(BlueprintCallable)
	bool IsJumping() const;
	UFUNCTION(BlueprintCallable)
	bool IsMoving() const;
	UFUNCTION(BlueprintCallable)
	bool IsFalling() const;
	
protected:
	virtual void BeginPlay() override;
	
	UFUNCTION(BlueprintImplementableEvent, Category="Collision")
	void MountWebLine(AActor* SurfaceActorPtr);
	
private:
	enum class State
	{
		Ground,
		Transition,
		Fall,
		OnWeb,
		Jumping
	};
	
	static constexpr float RAYCAST_LENGTH{ 1000.f };
	static constexpr float NORMAL_TOLERANCE{ 0.1f };

	// ==============================================================================
	// Member variables
	// ==============================================================================
	State m_State{ State::Fall };
	State m_OldState{ State::Fall };
	
	// Controls
	FVector m_Velocity{};
	
	// Player state
	bool m_HitWeb{ false };
	float m_JumpImmuneTimer{};

	// Collision
	FHitResult m_DownHitResult{};
	FHitResult m_ForwardHitResult{};

	// Lerp
	FVector m_StickPosition{};
	FRotator m_StickRotation{};
	float m_LerpTimer{ FLT_MAX };
	float m_LerpRatio{};

	// Web
	FVector m_StartLinePoint{};
	FVector m_EndLinePoint{};

	// ==============================================================================
	// States
	// ==============================================================================
	void Transition(float DeltaTime);
	void Grounded(float DeltaTime);
	void OnWeb(float DeltaTime);
	void Falling(float DeltaTime);
	void Jumping(float DeltaTime);
	
	// ==============================================================================
	// Player controlled action
	// ==============================================================================
	void RotateCamera();
	void RotateBody() const;
	void HandleMove(const FVector& Direction);
	UFUNCTION(BlueprintCallable, Category="Movement")
	void Jump(float JumpPower);

	// ==============================================================================
	// Physics
	// ==============================================================================
	void ApplyGravity(float DeltaTime);
	void ApplyDrag(float DeltaTime);

	// ==============================================================================
	// Surface sticking
	// ==============================================================================
	void SetTransition(const FHitResult& HitResult);
	void TransitionSurfaces(float DeltaTime);
	void CalcSurfaceStickPoint(const FHitResult& HitResult);
	void CalcLerpRatio();
	void StickToSurface();
	void RotateToWorld(float DeltaTime);
	
	// ==============================================================================
	// Hit detection
	// ==============================================================================
	bool CheckGrounded();
	bool CheckWall();
	bool CollisionPrediction(float DeltaTime);
	UFUNCTION(BlueprintCallable, Category="Collision")
	void SetClosestWeb(const FVector& Start, const FVector& End);
	bool IsOnWeb();
	
	// ==============================================================================
	// Helpers
	// ==============================================================================
	bool IsTransitioning() const;
	UFUNCTION(BlueprintCallable, Category="Helpers")
	bool HitWall() const;
	bool ChangedGround() const;
	bool FellOffWall() const;
	void PlaySound(UAudioComponent* Sound, bool Condition);
	void StopAllSounds();
	
	// ==============================================================================
	// Debug helpers
	// ==============================================================================
	void PrintRotation(const FRotator& Rotation) const;
	void PrintVector(const FVector& Vector, int key = -1) const;
	void PrintString(const FString& String, int key = -1) const;
	void PrintFloat(const FString& Label, float F) const;
	void DrawDebugArrow(const FVector& Location, const FVector& Direction, const FColor& Color, bool PersistentLines = false) const;
	
public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
};
