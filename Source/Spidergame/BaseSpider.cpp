// Fill out your copyright notice in the Description page of Project Settings.
#include "BaseSpider.h"

#include "Camera/CameraComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/SpringArmComponent.h"

// Sets default values
ABaseSpider::ABaseSpider()
{
 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Setup components
	Collider = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Collider"));
	RootComponent = Collider;

	// Setup scene component for relative rotation
	Spider = CreateDefaultSubobject<USceneComponent>(TEXT("Spider"));
	Spider->SetupAttachment(Collider);
	
	// Needs to be child to fix rotation errors with mesh
	SkeletalMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Skeletal Mesh"));
	SkeletalMesh->SetupAttachment(Spider);
	
	Arm = CreateDefaultSubobject<USpringArmComponent>(TEXT("Camera Arm"));
	Arm->SetupAttachment(Spider);
	
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Arm);
	
	WebSocket = CreateDefaultSubobject<UArrowComponent>(TEXT("WebSocket"));
	WebSocket->SetupAttachment(Spider);

	WalkSound = CreateDefaultSubobject<UAudioComponent>(TEXT("Walk"));
	WebWalkSound = CreateDefaultSubobject<UAudioComponent>(TEXT("WebWalk"));
	LandingSound = CreateDefaultSubobject<UAudioComponent>(TEXT("Land"));
}

// Called when the game starts or when spawned
void ABaseSpider::BeginPlay()
{
	Super::BeginPlay();
	StopAllSounds();
}

// Called every frame
void ABaseSpider::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	switch (m_State)
	{
	case State::Ground:
		Grounded(DeltaTime);
		break;
	case State::Transition:
		Transition(DeltaTime);
		break;
	case State::Fall:
		Falling(DeltaTime);
		break;
	case State::OnWeb:
		OnWeb(DeltaTime);
		break;
	case State::Jumping:
		Jumping(DeltaTime);
		break;
	default:
		Falling(DeltaTime);
		break;
	}

	ConsumeMovementInputVector(); // consume in case of buildup
	AddActorWorldOffset(m_Velocity * DeltaTime, true);
}

// Called to bind functionality to input
void ABaseSpider::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

#pragma region States
// ==============================================================================
// States
// ==============================================================================


void ABaseSpider::Transition(float DeltaTime)
{
	WalkSound->Stop();
	m_Velocity = {};
	
	if (!IsTransitioning())
	{
		m_State = IsOnWeb() ? State::OnWeb : State::Ground;
		StickToSurface();
		return;
	}
	
	TransitionSurfaces(DeltaTime);
	RotateCamera();
	
	
	m_OldState = State::Transition;
}

void ABaseSpider::Grounded(float DeltaTime)
{
	// Reset velocity later, used in state switching checking
	CheckGrounded();
	CheckWall();

	// Different ground check
	if (ChangedGround())
	{
		SetTransition(m_DownHitResult);
		m_State = State::Transition;
		m_Velocity = {};
		StopAllSounds();
		return;
	}

	if (HitWall() && IsMoving())
	{
		SetTransition(m_ForwardHitResult);
		m_State = State::Transition;
		m_Velocity = {};
		StopAllSounds();
		return;
	}
	
	if (!IsGrounded())
	{
		m_State = State::Fall;
		m_Velocity = {};
		StopAllSounds();
		return;
	}

	m_Velocity = {};
	
	// Turn camera + body
	RotateCamera();
	RotateBody();
	
	HandleMove(Spider->GetForwardVector());
	if (IsMoving())
	{
		StickToSurface();
		if (ChangedGround() && IsOnWeb())
		{
			m_State = State::OnWeb;
			m_Velocity = {};
			return;
		}
	}

	// Play web walking sound if on a big web
	const bool OnWeb{ m_DownHitResult.GetActor()->Tags.Contains(WebTag) };
	UAudioComponent* AudioPtr = (OnWeb) ? WebWalkSound : WalkSound;
	
	PlaySound(AudioPtr, m_Velocity.SquaredLength() > 0);
	
	m_OldState = State::Ground;
}

void ABaseSpider::OnWeb(float DeltaTime)
{
	// Reset velocity later, used in state switching checking
	CheckGrounded();
	CheckWall();
	
	if (!IsGrounded())
	{
		m_State = State::Fall;
		m_Velocity = {};
		StopAllSounds();
		return;
	}
	
	if (HitWall() && IsMoving())
	{
		SetTransition(m_ForwardHitResult);
		m_State = State::Transition;
		m_Velocity = {};
		StopAllSounds();
		return;
	}

	m_Velocity = {};
	
	RotateCamera();
	RotateBody();

	// Move on web back and forth based on looking direction
	FVector Direction{ m_EndLinePoint - m_StartLinePoint };
	Direction.Normalize();

	const double cosAngle{ Spider->GetForwardVector().Dot(Direction) };
	Direction *= FMath::Sign(cosAngle);
		
	HandleMove(Direction);

	PlaySound(WebWalkSound, m_Velocity.SquaredLength() > 0);
	
	m_OldState = State::OnWeb;
}

void ABaseSpider::Falling(float DeltaTime)
{
	WalkSound->Stop();
	
	if (CheckWall())
	{
		SetTransition(m_ForwardHitResult);
		m_State = State::Transition;
		m_Velocity = {};
		LandingSound->Play();
		return;
	}
	
	if (CollisionPrediction(DeltaTime))
	{
		m_Velocity = {};
		if(ChangedGround())
		{
			SetTransition(m_DownHitResult);
			m_State = State::Transition;
		}
		else
		{
			StickToSurface();
			m_State = State::Ground;
		}

		LandingSound->Play();
		return;
	}

	RotateToWorld(DeltaTime);

	// Standard falling
	RotateCamera();
	RotateBody();

	ApplyGravity(DeltaTime);
	ApplyDrag(DeltaTime);
	
	ConsumeMovementInputVector(); // consume in case of buildup
	m_OldState = State::Fall;
}

void ABaseSpider::Jumping(float DeltaTime)
{
	m_JumpImmuneTimer -= DeltaTime;
	if (m_JumpImmuneTimer < 0)
	{
		m_State = State::Fall;
		return;
	}
	RotateToWorld(DeltaTime);

	// Standard falling
	RotateCamera();
	RotateBody();

	ApplyGravity(DeltaTime);
	ApplyDrag(DeltaTime);
	
	m_OldState = State::Jumping;
}

#pragma endregion States

#pragma region PlayerControlledAction
// ==============================================================================
// Player controlled action
// ==============================================================================

void ABaseSpider::RotateCamera()
{
	const FRotator BodyRotation{ GetController()->GetControlRotation() };
	Camera->SetRelativeRotation( { BodyRotation.Pitch, 0 ,0} );
}

void ABaseSpider::RotateBody() const
{
	const FRotator BodyRotation{ GetController()->GetControlRotation() };
	Spider->SetRelativeRotation({0, BodyRotation.Yaw, 0});
}

void ABaseSpider::HandleMove(const FVector& Direction)
{
	const FVector Movement{ ConsumeMovementInputVector() };
	const double Length{ Movement.Y * MovementSpeed };
	
	m_Velocity += Direction * Length;
}

void ABaseSpider::Jump(float JumpPower)
{
	if (!CheckGrounded())
		return;

	// Jump more forward if moving forward
	if (m_Velocity.SquaredLength() != 0)
		m_Velocity.Normalize();
	
	m_Velocity += Spider->GetUpVector();
	m_Velocity.Normalize();
	m_Velocity *= JumpPower;
	
	m_JumpImmuneTimer = JumpImmuneTime;

	StopAllSounds();
	m_State = State::Jumping;
}

#pragma endregion PlayerControlledAction

#pragma region Physics
// ==============================================================================
// Physics
// ==============================================================================

void ABaseSpider::ApplyGravity(float DeltaTime)
{
	m_Velocity.Z -= Gravity * DeltaTime;
}

void ABaseSpider::ApplyDrag(float DeltaTime)
{
	m_Velocity -= m_Velocity * Drag * DeltaTime;
}
#pragma endregion Physics

#pragma region SurfaceSticking
// ==============================================================================
// Surface sticking
// ==============================================================================

void ABaseSpider::SetTransition(const FHitResult& HitResult)
{
	CalcSurfaceStickPoint(HitResult);
	CalcLerpRatio();
	m_LerpTimer = 0.f;
}

void ABaseSpider::TransitionSurfaces(float DeltaTime)
{
	// Ratio to get the correct Lerp values
	// Allows player to stop the lerp themselves
	const double LerpStep{ m_LerpRatio * DeltaTime / WallInterpolateTime };
	
	m_LerpTimer += LerpStep;
	
	Collider->AddWorldOffset(m_StickPosition * LerpStep);
	Collider->AddWorldRotation(m_StickRotation * LerpStep);
}

void ABaseSpider::CalcSurfaceStickPoint(const FHitResult& HitResult)
{
	FVector ImpactForward{ Spider->GetRightVector().Cross(HitResult.ImpactNormal) };
	ImpactForward.Normalize();
	// Size needs to be increased to position the player just above ground
	constexpr float SizeScalar{ 1.05f };
	const float Size { Collider->GetScaledCapsuleRadius() * SizeScalar };
	
	m_StickRotation = FQuat::FindBetweenVectors( Spider->GetUpVector(), HitResult.ImpactNormal ).Rotator();
	m_StickPosition = (HitResult.Location + HitResult.ImpactNormal * Size) - Collider->GetComponentLocation();
}

void ABaseSpider::CalcLerpRatio()
{
	// Change the lerp ratio to slow it down if the angle is too big
	// Avoid player nausea + snappy
	constexpr float CustomRatioThreshold{ 90.f };
	const double Angle{ FMath::Acos(Spider->GetUpVector().Dot(m_ForwardHitResult.ImpactNormal)) };

	if (Angle < CustomRatioThreshold)
		m_LerpRatio = 1.f / WallInterpolateTime;
	else
		m_LerpRatio = 1.f / (WallInterpolateTime * (CustomRatioThreshold / Angle) * LargeLerpCompensation); // Slows down lerps if an angle is too big
}

void ABaseSpider::StickToSurface()
{
	CalcSurfaceStickPoint(m_DownHitResult);
	Collider->AddWorldOffset(m_StickPosition);
	Collider->AddWorldRotation(m_StickRotation);
}

void ABaseSpider::RotateToWorld(float DeltaTime)
{
	if (FellOffWall())
	{
		m_StickRotation = FQuat::FindBetweenVectors(Spider->GetUpVector(), FVector::UnitZ()).Rotator();
		m_StickPosition = {};
		m_LerpTimer = 0.f;
		m_LerpRatio = 1.f;
	}
	if (IsTransitioning())
		TransitionSurfaces(DeltaTime);
}
#pragma endregion SurfaceSticking

#pragma region HitDetection
// ==============================================================================
// Hit detection
// ==============================================================================

bool ABaseSpider::CheckGrounded()
{
	// Trace downwards checking for anything
	const FVector Start{ GetActorLocation() };
	const FVector End{ Start - Spider->GetUpVector() * RAYCAST_LENGTH };

	// Raycast
	FCollisionQueryParams CollisionParams{};
	CollisionParams.AddIgnoredActor(this);
	GetWorld()->LineTraceSingleByChannel(m_DownHitResult, Start, End, ECC_Visibility, CollisionParams);
	
	return IsGrounded();
}

bool ABaseSpider::CheckWall()
{
	// Raycast with forward hit result instead of temp (HitWall uses forward hit result)
	FHitResult Temp{};
	
	// Trace forward checking for obstacls
	const FVector Start{ GetActorLocation() };
	const FVector End{ Start + Spider->GetForwardVector() * RAYCAST_LENGTH };

	// Raycast
	FCollisionQueryParams CollisionParams{};
	CollisionParams.AddIgnoredActor(this);
	GetWorld()->LineTraceSingleByChannel(m_ForwardHitResult, Start, End, ECollisionChannel::ECC_Visibility, CollisionParams);
	
	if (HitWall())
		return true;

	m_ForwardHitResult = Temp;
	return false;
}

bool ABaseSpider::CollisionPrediction(float DeltaTime)
{
	FHitResult HitResult{};
	FCollisionQueryParams CollisionParams{};
	CollisionParams.AddIgnoredActor(this);

	// Collision prediction
	GetWorld()->LineTraceSingleByChannel(HitResult, GetActorLocation(), GetActorLocation() +  m_Velocity * DeltaTime, ECC_Visibility, CollisionParams);
	
	// If the player would fall through the floor, stick them to the floor
	if (HitResult.IsValidBlockingHit())
	{
		m_DownHitResult = HitResult;
		return true;	
	}

	return false;
}

void ABaseSpider::SetClosestWeb(const FVector& Start, const FVector& End)
{	
	m_StartLinePoint = Start;
	m_EndLinePoint = End;
}
#pragma endregion HitDetection

#pragma region Helpers
// ==============================================================================
// Helpers
// ==============================================================================

bool ABaseSpider::IsTransitioning() const
{
	// Checking for 0 necessary due to going backwards
	return 0.f <= m_LerpTimer && m_LerpTimer <= 1.f;
}

bool ABaseSpider::IsGrounded() const
{
	return m_DownHitResult.IsValidBlockingHit() && m_DownHitResult.Distance <= GroundCheckDistance;
}

bool ABaseSpider::IsJumping() const
{
	return m_JumpImmuneTimer > 0.f;
}

bool ABaseSpider::HitWall() const
{
	return	m_ForwardHitResult.IsValidBlockingHit() &&
			m_ForwardHitResult.Distance <= WallCheckDistance;
}

bool ABaseSpider::IsOnWeb()
{
	AActor* Web{};
	
	if (CheckWall())
		Web = m_ForwardHitResult.GetActor();
	else if (CheckGrounded())
		Web = m_DownHitResult.GetActor();

	if (Web == nullptr || Web->Tags.IsEmpty())
		return false;

	if (Web->Tags.Contains(WebLineTag))
	{
		MountWebLine(Web);
		return true;
	}
	
	return false;
}

bool ABaseSpider::IsMoving() const
{
	return m_Velocity.Length() > FLT_EPSILON;
}

bool ABaseSpider::IsFalling() const
{
	return m_State == State::Fall;
}

bool ABaseSpider::ChangedGround() const
{
	return !GetActorUpVector().Equals(m_DownHitResult.ImpactNormal, NORMAL_TOLERANCE) && IsGrounded();
}

bool ABaseSpider::FellOffWall() const
{
	return !Spider->GetUpVector().Equals(FVector::UnitZ());
}

void ABaseSpider::PlaySound(UAudioComponent* Sound, bool Condition)
{
	if (Condition)
	{
		if (!Sound->IsPlaying())
			Sound->Play();
	}
	else
		Sound->Stop();
}

void ABaseSpider::StopAllSounds()
{
	WebWalkSound->Stop();
	WalkSound->Stop();
	LandingSound->Stop();
}

#pragma endregion Helpers

#pragma region Debug
// ==============================================================================
// Debug helpers
// ==============================================================================

void ABaseSpider::PrintRotation(const FRotator& Rotation) const
{
	GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Cyan,
			FString::Printf(TEXT( "Roll: %f, Yaw: %f, Pitch: %f"),
				Rotation.Roll,
				Rotation.Yaw,
				Rotation.Pitch));

}

void ABaseSpider::PrintVector(const FVector& Vector, int key) const
{
	GEngine->AddOnScreenDebugMessage(key, 2.f, FColor::Yellow,
			FString::Printf(TEXT( "X: %f, Y: %f, Z: %f"),
				Vector.X,
				Vector.Y,
				Vector.Z));
}

void ABaseSpider::PrintString(const FString& String, int key) const
{
	GEngine->AddOnScreenDebugMessage(key, 2.f, FColor::Silver, String);
}

void ABaseSpider::PrintFloat(const FString& Label, float F) const
{
	GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Purple,
			Label + FString::Printf(TEXT(": %f")
				, F));
}

void ABaseSpider::DrawDebugArrow(const FVector& Location, const FVector& Direction, const FColor& Color, bool PersistentLines) const
{
	constexpr float ArrowHeadSize{ 9.f };
	constexpr float ArrowSize{ 5.f };
	constexpr float LifeTIme{ 1.5f };
	DrawDebugDirectionalArrow(GetWorld(), Location, Location + Direction, ArrowHeadSize, Color, PersistentLines, LifeTIme, 0, ArrowSize);
}
#pragma endregion Debug