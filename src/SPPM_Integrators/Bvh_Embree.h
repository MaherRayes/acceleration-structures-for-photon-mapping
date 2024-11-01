
#if defined(_MSC_VER)
#define NOMINMAX
#pragma once
#endif

#ifndef BVHEMBREESPPMINTEGRATOR_H
#define BVHEMBREESPPMINTEGRATOR_H

#include "camera.h"
#include "film.h"
#include "integrator.h"
#include "pbrt.h"
#include "tbb/tbb.h"

namespace pbrt {
struct SPPMPixel;

// SPPM Declarations
class BVHSPPMIntegrator : public Integrator {
  public:
    // SPPMIntegrator Public Methods
    BVHSPPMIntegrator(std::shared_ptr<const Camera> &camera, int nIterations,
                     int photonsPerIteration, int maxDepth,
                     Float initialSearchRadius, int writeFrequency)
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
};

Integrator *CreateBVHSPPMIntegrator(const ParamSet &params,
                                   std::shared_ptr<const Camera> camera);

}  // namespace pbrt

#endif