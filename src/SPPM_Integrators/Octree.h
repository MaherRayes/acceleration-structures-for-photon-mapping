
#if defined(_MSC_VER)
#define NOMINMAX
#pragma once
#endif

#ifndef OCTREESPPMINTEGRATOR_H
#define OCTREESPPMINTEGRATOR_H
#include "camera.h"
#include "film.h"
#include "integrator.h"
#include "pbrt.h"
#include "tbb/tbb.h"

namespace pbrt {
class Octree;
struct SPPMPixel;

// SPPM Declarations
class OctreeSPPMIntegrator : public Integrator {
  public:
    // SPPMIntegrator Public Methods
    OctreeSPPMIntegrator(std::shared_ptr<const Camera> &camera,
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

class Octree {
  public:
    // Integrator Interface
    Octree(std::vector<SPPMPixel *> *pixels, int threshold, int depth)
        : _points(pixels), threshold(threshold), depth(depth) {}

    ~Octree() {
        if (!leaf) {
            for (int i = 0; i < 8; i++) delete _child[i];
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
    Octree *_child[8];
    int depth;
    int threshold;
    std::vector<SPPMPixel *> *_points;
    std::vector<int> assigned_points;
    bool leaf = false;
    Bounds3f treeBounds;
    Point3f c;
};

Integrator *CreateOctreeSPPMIntegrator(
    const ParamSet &params, std::shared_ptr<const Camera> camera);

}  // namespace pbrt

#endif