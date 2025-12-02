#pragma once
#include "PrimitiveComponent.h"
#include "UEContainer.h"

class URenderer;
struct FTrianglesBatch;

/**
 * UTriangleMeshComponent - 삼각형 메시 렌더링을 위한 컴포넌트
 *
 * 디버그 시각화 및 에디터용 메시 렌더링에 사용됩니다.
 * Physics Asset Editor에서 바디의 solid shape 표현 등에 활용됩니다.
 */
class UTriangleMeshComponent : public UPrimitiveComponent
{
public:
    DECLARE_CLASS(UTriangleMeshComponent, UPrimitiveComponent)

    UTriangleMeshComponent();
    virtual ~UTriangleMeshComponent() override;

public:
    // Mesh data management
    void SetMesh(const TArray<FVector>& Vertices, const TArray<uint32>& Indices, const TArray<FVector4>& Colors);
    void SetMesh(const FTrianglesBatch& Batch);
    void ClearMesh();

    // Rendering
    void CollectBatches(URenderer* Renderer);
    bool HasVisibleMesh() const { return bMeshVisible && !Vertices.empty(); }

    // Visibility control
    void SetMeshVisible(bool bVisible) { bMeshVisible = bVisible; }
    bool IsMeshVisible() const { return bMeshVisible; }

    // Overlay behavior
    void SetAlwaysOnTop(bool bInAlwaysOnTop) { bAlwaysOnTop = bInAlwaysOnTop; }
    bool IsAlwaysOnTop() const { return bAlwaysOnTop; }

    // Data access
    const TArray<FVector>& GetVertices() const { return Vertices; }
    const TArray<uint32>& GetIndices() const { return Indices; }
    const TArray<FVector4>& GetColors() const { return Colors; }

    // Duplication
    void DuplicateSubObjects() override;
    DECLARE_DUPLICATE(UTriangleMeshComponent)

private:
    TArray<FVector> Vertices;
    TArray<uint32> Indices;
    TArray<FVector4> Colors;

    bool bMeshVisible = true;
    bool bAlwaysOnTop = false;
};
