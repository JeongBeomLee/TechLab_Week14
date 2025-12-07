// ────────────────────────────────────────────────────────────────────────────
// ItemCollectGameMode.cpp
// 아이템 수집 게임 모드 구현
// ────────────────────────────────────────────────────────────────────────────
#include "pch.h"
#include "ItemCollectGameMode.h"
#include "FirefighterCharacter.h"
#include "World.h"
#include "PlayerController.h"
#include "Pawn.h"

// ────────────────────────────────────────────────────────────────────────────
// 생성자
// ────────────────────────────────────────────────────────────────────────────

AItemCollectGameMode::AItemCollectGameMode()
	: PlayerSpawnScale(1.5f)
	, SpawnActorName("PlayerStartPos")
	, SpawnActorTag("None")
{
	DefaultPawnClass = AFirefighterCharacter::StaticClass();
	PlayerSpawnLocation = FVector(0.0f, 0.0f, 1.0f);
}

// ────────────────────────────────────────────────────────────────────────────
// 생명주기
// ────────────────────────────────────────────────────────────────────────────

void AItemCollectGameMode::BeginPlay()
{
	Super::BeginPlay();
}

// ────────────────────────────────────────────────────────────────────────────
// 플레이어 초기화
// ────────────────────────────────────────────────────────────────────────────

void AItemCollectGameMode::InitPlayer()
{
	// 1. PlayerController 생성
	PlayerController = SpawnPlayerController();

	if (!PlayerController)
	{
		UE_LOG("[error] ItemCollectGameMode::InitPlayer - Failed to spawn PlayerController");
		return;
	}

	// 2. 스폰 위치 결정 (Actor 기준 또는 기본 위치)
	FVector SpawnLocation = PlayerSpawnLocation;
	FQuat SpawnRotation = FQuat::Identity();

	// SpawnActor를 찾아서 위치 가져오기
	AActor* SpawnActor = FindSpawnActor();
	if (SpawnActor)
	{
		SpawnLocation = SpawnActor->GetActorLocation();
		SpawnRotation = SpawnActor->GetActorRotation();
		UE_LOG("[info] ItemCollectGameMode::InitPlayer - Using SpawnActor location: (%.2f, %.2f, %.2f)",
			SpawnLocation.X, SpawnLocation.Y, SpawnLocation.Z);
	}

	// 3. 스폰 Transform 설정 (스케일은 SetCharacterScale에서 처리)
	FTransform SpawnTransform;
	SpawnTransform.Translation = SpawnLocation;
	SpawnTransform.Rotation = SpawnRotation;
	SpawnTransform.Scale3D = FVector(1.0f, 1.0f, 1.0f);  // 기본 스케일로 스폰

	// 4. Pawn 스폰 및 빙의
	UE_LOG("[info] ItemCollectGameMode::InitPlayer - DefaultPawnClass: %s",
		DefaultPawnClass ? DefaultPawnClass->Name : "nullptr");

	APawn* SpawnedPawn = SpawnDefaultPawnFor(PlayerController, SpawnTransform);

	if (!SpawnedPawn)
	{
		UE_LOG("[error] ItemCollectGameMode::InitPlayer - Failed to spawn DefaultPawn");
	}
	else
	{
		// 5. FirefighterCharacter의 스케일 설정 (캡슐, 메쉬, 스프링암 등 모두 조절)
		if (AFirefighterCharacter* Firefighter = Cast<AFirefighterCharacter>(SpawnedPawn))
		{
			Firefighter->SetCharacterScale(PlayerSpawnScale);
		}

		UE_LOG("[info] ItemCollectGameMode::InitPlayer - Spawned Pawn: %s with scale %.2f",
			SpawnedPawn->GetClass()->Name, PlayerSpawnScale);
	}
}

AActor* AItemCollectGameMode::FindSpawnActor() const
{
	if (!World)
	{
		return nullptr;
	}

	// 1. SpawnActorName으로 먼저 찾기
	if (SpawnActorName != "None")
	{
		AActor* FoundActor = World->FindActorByName(SpawnActorName);
		if (FoundActor)
		{
			UE_LOG("[info] ItemCollectGameMode::FindSpawnActor - Found actor by name: %s",
				SpawnActorName.ToString().c_str());
			return FoundActor;
		}
		else
		{
			UE_LOG("[warning] ItemCollectGameMode::FindSpawnActor - Actor not found by name: %s",
				SpawnActorName.ToString().c_str());
		}
	}

	// 2. SpawnActorTag로 찾기 (모든 Actor 순회)
	if (SpawnActorTag != "None")
	{
		FString TagToFind = SpawnActorTag.ToString();
		for (AActor* Actor : World->GetActors())
		{
			if (Actor && Actor->GetTag() == TagToFind)
			{
				UE_LOG("[info] ItemCollectGameMode::FindSpawnActor - Found actor by tag: %s",
					TagToFind.c_str());
				return Actor;
			}
		}
		UE_LOG("[warning] ItemCollectGameMode::FindSpawnActor - Actor not found by tag: %s",
			TagToFind.c_str());
	}

	return nullptr;
}
