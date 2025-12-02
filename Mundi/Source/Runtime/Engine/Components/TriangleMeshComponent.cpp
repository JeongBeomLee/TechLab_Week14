#include "pch.h"
#include "TriangleMeshComponent.h"
#include "LinesBatch.h"
#include "Renderer.h"

IMPLEMENT_CLASS(UTriangleMeshComponent)

UTriangleMeshComponent::UTriangleMeshComponent()
    : UPrimitiveComponent()
{
}

UTriangleMeshComponent::~UTriangleMeshComponent()
{
}

void UTriangleMeshComponent::SetMesh(const TArray<FVector>& InVertices, const TArray<uint32>& InIndices, const TArray<FVector4>& InColors)
{
    Vertices = InVertices;
    Indices = InIndices;
    Colors = InColors;
}

void UTriangleMeshComponent::SetMesh(const FTrianglesBatch& Batch)
{
    Vertices = Batch.Vertices;
    Indices = Batch.Indices;
    Colors = Batch.Colors;
}

void UTriangleMeshComponent::ClearMesh()
{
    Vertices.clear();
    Indices.clear();
    Colors.clear();
}

void UTriangleMeshComponent::CollectBatches(URenderer* Renderer)
{
    if (!HasVisibleMesh() || !Renderer)
        return;

    Renderer->BeginTriangleBatch();
    Renderer->AddTriangles(Vertices, Indices, Colors);

    if (bAlwaysOnTop)
    {
        Renderer->EndTriangleBatchAlwaysOnTop(FMatrix::Identity());
    }
    else
    {
        Renderer->EndTriangleBatch(FMatrix::Identity());
    }
}

void UTriangleMeshComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
    // Mesh data는 값 타입이므로 자동으로 복사됨
}
