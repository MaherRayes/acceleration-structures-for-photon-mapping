/*
#if defined(_MSC_VER)
#define NOMINMAX
#pragma once
#endif

#ifndef SPPMACCELERATOR_H
#define SPPMACCELERATOR_H

#include "pbrt.h"
#include "SPPM_Integrators/SAH_InPlace_KD_par.h"
#include "geometry.h"
//#include "integrators/kdsppm.cpp"

namespace pbrt {

// Integrator Declarations
class SPPMAccelerator {
  public:
    // Integrator Interface
    //virtual ~SPPMAccelerator();
    virtual void build() = 0;
    virtual std::vector<int>* trace(Point3f p) = 0;
};
}  // namespace pbrt

#endif*/