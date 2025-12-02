#pragma once
#include "PrimitiveComponent.h"
#include "Line.h"
#include "UEContainer.h"

class URenderer;
struct FLinesBatch;
struct FTrianglesBatch;

class ULineComponent : public UPrimitiveComponent
{
public:
    DECLARE_CLASS(ULineComponent, UPrimitiveComponent)

    ULineComponent();
    virtual ~ULineComponent() override;

public:
    // Line management (ULine 기반 - 기존 호환성)
    ULine* AddLine(const FVector& StartPoint, const FVector& EndPoint, const FVector4& Color = FVector4(1,1,1,1));
    void RemoveLine(ULine* Line);
    void ClearLines();

    void CollectLineBatches(URenderer* Renderer);

    // Properties
    void SetLineVisible(bool bVisible) { bLinesVisible = bVisible; }
    bool IsLineVisible() const { return bLinesVisible; }

    const TArray<ULine*>& GetLines() const { return Lines; }
    int64 GetLineCount() const { return static_cast<int64>(Lines.size()); }

    // Efficient world coordinate line data extraction
    void GetWorldLineData(TArray<FVector>& OutStartPoints, TArray<FVector>& OutEndPoints, TArray<FVector4>& OutColors) const;
    bool HasVisibleLines() const { return bLinesVisible && (!Lines.empty() || !DirectStartPoints.empty()); }

    // Overlay behavior
    void SetAlwaysOnTop(bool bInAlwaysOnTop) { bAlwaysOnTop = bInAlwaysOnTop; }
    bool IsAlwaysOnTop() const { return bAlwaysOnTop; }

    // ───── Direct Batch Mode (DOD 기반 - ULine 없이 직접 배열 사용) ─────
    void SetDirectBatch(const TArray<FVector>& StartPoints, const TArray<FVector>& EndPoints, const TArray<FVector4>& Colors);
    void SetDirectBatch(const FLinesBatch& Batch);
    void ClearDirectBatch();
    TArray<FVector>& GetDirectStartPoints() { return DirectStartPoints; }
    TArray<FVector>& GetDirectEndPoints() { return DirectEndPoints; }
    TArray<FVector4>& GetDirectColors() { return DirectColors; }


    // ───── 복사 관련 ────────────────────────────
    void DuplicateSubObjects() override;
    DECLARE_DUPLICATE(ULineComponent)

private:
    TArray<ULine*> Lines;
    bool bLinesVisible = true;
    bool bAlwaysOnTop = false;

    // Direct batch data (DOD mode) - Lines
    TArray<FVector> DirectStartPoints;
    TArray<FVector> DirectEndPoints;
    TArray<FVector4> DirectColors;
 };
