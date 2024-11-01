
#if defined(_MSC_VER)
#define NOMINMAX
#pragma once
#endif

#ifndef NESTEDGRIDPARSPPMINTEGRATOR_H
#define NESTEDGRIDPARSPPMINTEGRATOR_H
#include "camera.h"
#include "film.h"
#include "integrator.h"
#include "pbrt.h"
#include "tbb/tbb.h"

namespace pbrt {
class NestedGrid;
struct SPPMPixel;

// SPPM Declarations
class NestedGridParSPPMIntegrator : public Integrator {
  public:
    // SPPMIntegrator Public Methods
    NestedGridParSPPMIntegrator(std::shared_ptr<const Camera> &camera,
                                 int nIterations,
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

class NestedGridPar {
  public:
    // Integrator Interface
    NestedGridPar(std::vector<SPPMPixel *> *pixels, int base, float maxRadius,
                   int threshold, int depth)
        : _points(pixels),
          base(base),
          maxRadius(maxRadius),
          threshold(threshold),
          depth(depth),
          child_count(base * base * base) {}

    ~NestedGridPar() {
        if (!leaf) {
            for (int i = 0; i < child_count; i++) delete _child[i];
        }
    }

    inline const void addPoint(int s) { assigned_points.push_back(s); }
    inline const void setBounds(Bounds3f b) { treeBounds = b; }
    inline const unsigned int pointCount() const {
        return assigned_points.size();
    }

    void build();
    std::vector<int> *trace(Point3f p);

  protected:
    std::vector<SPPMPixel *> *_points;
    float maxRadius;
    int threshold;
    int depth;
    int base;
    std::vector<NestedGridPar *> _child;
    int child_count;
    std::vector<int> assigned_points;
    bool leaf = false;
    Bounds3f treeBounds;

    float x_add;
    float y_add;
    float z_add;
};

Integrator *CreateNestedGridParSPPMIntegrator(
    const ParamSet &params,
                                   std::shared_ptr<const Camera> camera);

}  // namespace pbrt

#endif