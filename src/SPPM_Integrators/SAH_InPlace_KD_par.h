
#if defined(_MSC_VER)
#define NOMINMAX
#pragma once
#endif

#ifndef SAHINPLACEKDPAR_H
#define SAHINPLACEKDPAR_H

#include "camera.h"
#include "film.h"
#include "integrator.h"
#include "pbrt.h"

namespace pbrt {


    // SPPM Local Definitions
struct SPPMPixel {
    // SPPMPixel Public Methods
    SPPMPixel() : M(0) {}

    // SPPMPixel Public Methods
    Bounds3f WorldBound() const {
        Bounds3f b(vp.p);
        b.pMin = vp.p - Vector3f(radius, radius, radius);
        b.pMax = vp.p + Vector3f(radius, radius, radius);
        return b;
    }

    // SPPMPixel Public Data
    Float radius = 0;
    Spectrum Ld;
    struct VisiblePoint {
        // VisiblePoint Public Methods
        VisiblePoint() {}
        VisiblePoint(const Point3f &p, const Vector3f &wo, const BSDF *bsdf,
                     const Spectrum &beta)
            : p(p), wo(wo), bsdf(bsdf), beta(beta) {}
        Point3f p;
        Vector3f wo;
        const BSDF *bsdf = nullptr;
        Spectrum beta;
    } vp;
    AtomicFloat Phi[Spectrum::nSamples];
    std::atomic<int> M;
    Float N = 0;
    Spectrum tau;
};

// SPPM Declarations
class SAHInPlaceKDParSPPMIntegrator : public Integrator {
  public:
    // SPPMIntegrator Public Methods
    SAHInPlaceKDParSPPMIntegrator(std::shared_ptr<const Camera> &camera,
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
};

Integrator *CreateSAHInPlaceKDParSPPMIntegrator(
    const ParamSet &params, std::shared_ptr<const Camera> camera);

}  // namespace pbrt
#endif