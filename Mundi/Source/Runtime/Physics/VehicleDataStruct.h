#pragma once
#include "PxPhysicsAPI.h"
#include "Vector.h"

struct FVehicleClutchData
{
	float Strength = 10.0f;
};

// 엔진 토크 커브 데이터 (RPM에 따른 토크 변화)
struct FVehicleEngineData
{
	float PeakTorque = 500.0f;    // 최대 토크 (Nm)
	float MaxRPM = 6000.0f;       // 최대 회전수
	// 토크 커브 (0~1 정규화된 RPM에 대한 토크 비율)
	// 실제로는 CurveFloat 등을 사용하겠지만, 여기선 단순화

	float GetMaxOmega()
	{
		return (TWO_PI * MaxRPM) / 60.0f;
	}
};

// 기어비 데이터
struct FVehicleGearboxData
{
	float SwitchTime = 0.5f;      // 기어 변경 시간 (초)
	float FinalRatio = 4.0f;      // 최종 감속비
	// 기어별 비율 (후진, 중립, 1단, 2단, ...)
	TArray<float> Gears = { -4.0f, 0.0f, 4.0f, 2.0f, 1.5f, 1.1f, 1.0f };
};

// 휠 및 서스펜션 데이터 (바퀴마다 다를 수 있으나 보통 앞/뒤로 구분)
struct FWheelSuspensionData
{
	float Radius = 0.5f;          // 바퀴 반지름
	float Width = 0.4f;           // 바퀴 폭
	float Mass = 20.0f;           // 바퀴 무게
	float DampingRate = 0.25f;    // 바퀴 회전 감쇠(저항)
	float MaxBrakeTorque = 1500.0f; // 브레이크 힘
	float MaxHandBrakeTorque = 4000.0f; // 핸드브레이크 힘

	// 서스펜션 핵심
	float MaxCompression = 0.3f;  // 최대 압축 (m)
	float MaxDroop = 0.1f;        // 최대 늘어짐 (m)
	float SpringStrength = 35000.0f; // 스프링 강도
	float SpringDamper = 4500.0f;    // 댐퍼 강도

	// NOTE: 추후 캐싱 처리
	PxF32 GetWheelMOI()
	{
		return 0.5f * Mass * Radius * Radius;
	}
};

// 최종 차량 데이터 에셋
struct FVehicleData
{
	// 차체(Chassis)
	float ChassisMass = 1500.0f;
	physx::PxVec3 ChassisDims = { 2.5f, 2.0f, 5.0f };
	physx::PxVec3 CenterOfMassOffset = { 0.0f, -0.25f, 0.0f }; // 바닥 기준이 아닌 중심 기준 오프셋

	// 부품 데이터
	FVehicleEngineData Engine;
	FVehicleGearboxData Gearbox;
	FWheelSuspensionData Wheel;
	FVehicleClutchData Clutch;

	// NOTE: 추후 캐싱 처리
	PxVec3 GetChassisMOI()
	{
		//Now compute the chassis mass and moment of inertia.
		//Use the moment of inertia of a cuboid as an approximate value for the chassis moi.
		PxVec3 chassisMOI
		((ChassisDims.y * ChassisDims.y + ChassisDims.z * ChassisDims.z) * ChassisMass / 12.0f,
			(ChassisDims.x * ChassisDims.x + ChassisDims.z * ChassisDims.z) * ChassisMass / 12.0f,
			(ChassisDims.x * ChassisDims.x + ChassisDims.y * ChassisDims.y) * ChassisMass / 12.0f);
		//A bit of tweaking here.  The car will have more responsive turning if we reduce the 	
		//y-component of the chassis moment of inertia.
		chassisMOI.y *= 0.8f;

		return chassisMOI;
	}
};