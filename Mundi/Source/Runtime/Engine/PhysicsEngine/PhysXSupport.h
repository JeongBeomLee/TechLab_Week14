#pragma once

#include "PxPhysicsAPI.h"

using namespace physx;

/**
 * @note 현재는 PhysX에서 제공하는 디폴트 할당자를 사용한다.
 * @todo 별도의 메모리 할당 로직이 필요할 경우 'allocate()'를 오버라이딩해서 사용한다.
 */
class FPhysXAllocator : public PxDefaultAllocator
{
};

/**
 * PhysX 에러를 Future Engine의 로그 시스템으로 가져온다.
 */
class FPhysXErrorCallback : public PxErrorCallback
{
public:
    virtual void reportError(PxErrorCode::Enum code, const char* message, const char* file, int line) override
    {
        std::string ErrorCodeStr;
        switch (code)
        {
        case PxErrorCode::eNO_ERROR:          ErrorCodeStr = "Info"; break;
        case PxErrorCode::eDEBUG_INFO:        ErrorCodeStr = "Debug"; break;
        case PxErrorCode::eDEBUG_WARNING:     ErrorCodeStr = "Warning"; break;
        case PxErrorCode::eINVALID_PARAMETER: ErrorCodeStr = "Invalid Param"; break;
        case PxErrorCode::eINVALID_OPERATION: ErrorCodeStr = "Invalid Op"; break;
        case PxErrorCode::eOUT_OF_MEMORY:     ErrorCodeStr = "Out of Memory"; break;
        case PxErrorCode::eINTERNAL_ERROR:    ErrorCodeStr = "Internal Error"; break;
        default:                                     ErrorCodeStr = "Unknown"; break;
        }

        UE_LOG("[PhysX Error] [%s] %s (%s:%d)\n", ErrorCodeStr.c_str(), message, file, line);
    }
};

// ==================================================================================
// GLOBAL POINTERS
// ==================================================================================

/** PhysX Foundation 싱글톤에 대한 포인터 */
extern PxFoundation*            GPhysXFoundation;

extern PxPhysics*               GPhysXSDK;

extern PxCooking*               GPhysXCooking;

extern PxDefaultCpuDispatcher*  GPhysXDispatcher;

/** PhysX 디버거에 대한 포인터 */
extern PxPvd*                   GPhysXVisualDebugger;

extern FPhysXAllocator*         GPhysXAllocator;

extern FPhysXErrorCallback*     GPhysXErrorCallback;

bool InitGamePhys();
void TermGamePhys();