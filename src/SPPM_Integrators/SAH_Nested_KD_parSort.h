
#if defined(_MSC_VER)
#define NOMINMAX
#pragma once
#endif

#ifndef SAHNESTEDKDPARSPPMINTEGRATOR_H
#define SAHNESTEDKDPARSPPMINTEGRATOR_H

#include "camera.h"
#include "film.h"
#include "integrator.h"
#include "pbrt.h"

namespace pbrt {

struct KdAccelNode;
struct BoundEdge;

// SPPM Declarations
class SAHNestedKDParSPPMIntegrator : public Integrator {
  public:
    // SPPMIntegrator Public Methods
    SAHNestedKDParSPPMIntegrator(std::shared_ptr<const Camera> &camera,
                              int nIterations, int photonsPerIteration,
                              int maxDepth, Float initialSearchRadius,
                              int writeFrequency)
        : camera(camera),
          initialSearchRadius(initialSearchRadius),
          nIterations(nIterations),
          maxDepth(maxDepth),
          photonsPerIteration(photonsPerIteration > 0
                                  ? photonsPerIteration
                                  : camera->film->croppedPixelBounds.Area()),
          writeFrequency(writeFrequency) {}
    void Render(const Scene &scene);

  private:
    // SPPMIntegrator Private Data
    std::shared_ptr<const Camera> camera;
    const Float initialSearchRadius;
    const int nIterations;
    const int maxDepth;
    const int photonsPerIteration;
    const int writeFrequency;
    // KDtree

    int nextFreeNode;
    int nAllocedNodes;
    int isectCost;
    int traversalCost;
    Float emptyBonus;
    int maxvis = 1;
    float maxD;
    std::vector<int> visPointsIndices;
    Bounds3f bounds;

    void buildTree(int nodeNum, const Bounds3f &bounds,
                   const std::vector<Bounds3f> &primBounds, int *primNums,
                   int nprims, int depth,
                   const std::unique_ptr<BoundEdge[]> edges[3], int *prims0,
                   int *prims1, KdAccelNode *&nodes, int badRefines = 0);
};

Integrator *CreateSAHNestedKDParSPPMIntegrator(
    const ParamSet &params, std::shared_ptr<const Camera> camera);

}  // namespace pbrt
#endif