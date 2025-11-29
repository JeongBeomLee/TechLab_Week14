#include "pch.h"
#include "BodySetup.h"

UBodySetup::UBodySetup()
{
    if (GPhysXSDK)
    {
        // @todo 임시 머티리얼 사용, 이후 별도의 머티리얼 관리자가 필요함
        PhysMaterial = GPhysXSDK->createMaterial(0.5f, 0.5f, 0.5f);
    }
}

UBodySetup::~UBodySetup()
{
    // PhysX 머티리얼 해제 (레퍼런스 카운팅)
    if (PhysMaterial)
    {
        PhysMaterial->release();
        PhysMaterial = nullptr;
    }
}

void UBodySetup::AddShapesToRigidActor_AssumesLocked(FBodyInstance* OwningInstance, const FVector& Scale3D, PxRigidActor* PDestActor)
{
    if (!PDestActor || !GPhysXSDK)
    {
        return;
    }

    PxMaterial* MaterialToUse = PhysMaterial;
    if (!MaterialToUse)
    {
        return; 
    }

    for (const FKBoxElem& BoxElem : AggGeom.BoxElems)
    {
        if (BoxElem.GetCollisionEnabled() == ECollisionEnabled::NoCollision)
        {
            continue;
        }

        PxBoxGeometry BoxGeom = BoxElem.GetPxGeometry(Scale3D);

        PxShape* NewShape = GPhysXSDK->createShape(BoxGeom, *MaterialToUse, true);

        if (NewShape)
        {
            FTransform ElemTM = BoxElem.GetTransform();
            
            FVector ScaledCenter;
            ScaledCenter.X = ElemTM.Translation.X * Scale3D.X;
            ScaledCenter.Y = ElemTM.Translation.Y * Scale3D.Y;
            ScaledCenter.Z = ElemTM.Translation.Z * Scale3D.Z;

            PxTransform PxLocalPose(U2PVector(ScaledCenter), U2PQuat(ElemTM.Rotation));
            NewShape->setLocalPose(PxLocalPose);

            if (BoxElem.GetCollisionEnabled() == ECollisionEnabled::QueryOnly)
            {
                NewShape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
                NewShape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, true);
            }
            else if (BoxElem.GetCollisionEnabled() == ECollisionEnabled::PhysicsOnly)
            {
                NewShape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
                NewShape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, false);
            }
            else
            {
                NewShape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
                NewShape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, true);
            }

            PDestActor->attachShape(*NewShape);
            
            NewShape->release();
        }
    }
}

void UBodySetup::AddBox(const FVector& Extent)
{
    FKBoxElem Box;
    Box.X = Extent.X;
    Box.Y = Extent.Y;
    Box.Z = Extent.Z;
    AggGeom.BoxElems.Add(Box);
}

