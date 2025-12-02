#include "pch.h"
#include "PhysicsPickingHelper.h"
#include "Source/Runtime/Engine/PhysicsEngine/BodySetup.h"
#include "Vector.h"

namespace PhysicsPickingHelper
{
	bool IntersectRayBodyWithPrimitive(
		const FRay& WorldRay,
		const UBodySetup* Body,
		const FTransform& BoneWorldTransform,
		FPrimitiveHitResult& OutResult)
	{
		if (!Body) return false;

		OutResult.Clear();

		// Test all Sphere shapes
		for (int32 i = 0; i < Body->AggGeom.SphereElems.Num(); ++i)
		{
			const FKSphereElem& Sphere = Body->AggGeom.SphereElems[i];
			FVector WorldCenter = BoneWorldTransform.Translation + BoneWorldTransform.Rotation.RotateVector(Sphere.Center);
			float HitT;
			if (IntersectRaySphere(WorldRay, WorldCenter, Sphere.Radius, HitT))
			{
				if (HitT < OutResult.HitT)
				{
					OutResult.HitT = HitT;
					OutResult.PrimitiveType = EAggCollisionShape::Sphere;
					OutResult.PrimitiveIndex = i;
				}
			}
		}

		// Test all Box shapes
		for (int32 i = 0; i < Body->AggGeom.BoxElems.Num(); ++i)
		{
			const FKBoxElem& Box = Body->AggGeom.BoxElems[i];
			FVector WorldCenter = BoneWorldTransform.Translation + BoneWorldTransform.Rotation.RotateVector(Box.Center);
			FQuat WorldRotation = BoneWorldTransform.Rotation * Box.Rotation;
			FVector HalfExtent(Box.X * 0.5f, Box.Y * 0.5f, Box.Z * 0.5f);
			float HitT;
			if (IntersectRayOBB(WorldRay, WorldCenter, HalfExtent, WorldRotation, HitT))
			{
				if (HitT < OutResult.HitT)
				{
					OutResult.HitT = HitT;
					OutResult.PrimitiveType = EAggCollisionShape::Box;
					OutResult.PrimitiveIndex = i;
				}
			}
		}

		// Test all Capsule (Sphyl) shapes
		for (int32 i = 0; i < Body->AggGeom.SphylElems.Num(); ++i)
		{
			const FKSphylElem& Capsule = Body->AggGeom.SphylElems[i];
			FVector WorldCenter = BoneWorldTransform.Translation + BoneWorldTransform.Rotation.RotateVector(Capsule.Center);
			FQuat WorldRotation = BoneWorldTransform.Rotation * Capsule.Rotation;
			// Capsule is aligned along local Z-axis (engine convention)
			FVector LocalAxis = FVector(0.0f, 0.0f, 1.0f);
			FVector WorldAxis = WorldRotation.RotateVector(LocalAxis);
			float HalfLength = Capsule.Length * 0.5f;
			FVector CapsuleStart = WorldCenter - WorldAxis * HalfLength;
			FVector CapsuleEnd = WorldCenter + WorldAxis * HalfLength;
			float HitT;
			if (IntersectRayCapsule(WorldRay, CapsuleStart, CapsuleEnd, Capsule.Radius, HitT))
			{
				if (HitT < OutResult.HitT)
				{
					OutResult.HitT = HitT;
					OutResult.PrimitiveType = EAggCollisionShape::Sphyl;
					OutResult.PrimitiveIndex = i;
				}
			}
		}

		return OutResult.IsValid();
	}
}
