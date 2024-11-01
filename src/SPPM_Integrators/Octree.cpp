
#include <omp.h>
#include <stdlib.h>

#include <algorithm>
#include <chrono>

#include "SPPM_Integrators/Octree.h"
#include "imageio.h"
#include "interaction.h"
#include "parallel.h"
#include "paramset.h"
#include "progressreporter.h"
#include "rng.h"
#include "samplers/halton.h"
#include "sampling.h"
#include "scene.h"
#include "spectrum.h"
#include "stats.h"

namespace pbrt {

STAT_RATIO(
    "Stochastic Progressive Photon Mapping/Visible points checked per photon "
    "intersection",
    visiblePointsChecked, totalPhotonSurfaceInteractions);
STAT_COUNTER("Stochastic Progressive Photon Mapping/Photon paths followed",
             photonPaths);
STAT_INT_DISTRIBUTION(
    "Stochastic Progressive Photon Mapping/Grid cells per visible point",
    gridCellsPerVisiblePoint);
STAT_MEMORY_COUNTER("Memory/SPPM Pixels", pixelMemoryBytes);
STAT_FLOAT_DISTRIBUTION("Memory/SPPM BSDF and Grid Memory", memoryArenaMB);

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
        unsigned int code = 0;
        Vector3f wo;
        const BSDF *bsdf = nullptr;
        Spectrum beta;
    } vp;
    AtomicFloat Phi[Spectrum::nSamples];
    std::atomic<int> M;
    Float N = 0;
    Spectrum tau;
};


// SPPM Method Definitions
void OctreeSPPMIntegrator::Render(const Scene &scene) {
    auto t5 = std::chrono::high_resolution_clock::now();
    ProfilePhase p(Prof::IntegratorRender);
    // Initialize _pixelBounds_ and _pixels_ array for SPPM
    Bounds2i pixelBounds = camera->film->croppedPixelBounds;
    int nPixels = pixelBounds.Area();
    std::unique_ptr<SPPMPixel[]> pixels(new SPPMPixel[nPixels]);
    for (int i = 0; i < nPixels; ++i) pixels[i].radius = initialSearchRadius;
    const Float invSqrtSPP = 1.f / std::sqrt(nIterations);
    pixelMemoryBytes = nPixels * sizeof(SPPMPixel);
    // Compute _lightDistr_ for sampling lights proportional to power
    std::unique_ptr<Distribution1D> lightDistr =
        ComputeLightPowerDistribution(scene);

    // Perform _nIterations_ of SPPM integration
    HaltonSampler sampler(nIterations, pixelBounds);

    // Compute number of tiles to use for SPPM camera pass
    Vector2i pixelExtent = pixelBounds.Diagonal();
    const int tileSize = 16;
    Point2i nTiles((pixelExtent.x + tileSize - 1) / tileSize,
                   (pixelExtent.y + tileSize - 1) / tileSize);
    ProgressReporter progress(2 * nIterations, "Rendering");
    std::vector<MemoryArena> perThreadArenas(MaxThreadIndex());

    std::cout << std::fixed;
    float buildtime = 0;
    float tracetime = 0;
    std::cout << "sadsaedasdas" << std::endl;
    for (int iter = 0; iter < nIterations; ++iter) {
        // Generate SPPM visible points
        {
            ProfilePhase _(Prof::SPPMCameraPass);
            ParallelFor2D(
                [&](Point2i tile) {
                    MemoryArena &arena = perThreadArenas[ThreadIndex];
                    // Follow camera paths for _tile_ in image for SPPM
                    int tileIndex = tile.y * nTiles.x + tile.x;
                    std::unique_ptr<Sampler> tileSampler =
                        sampler.Clone(tileIndex);

                    // Compute _tileBounds_ for SPPM tile
                    int x0 = pixelBounds.pMin.x + tile.x * tileSize;
                    int x1 = std::min(x0 + tileSize, pixelBounds.pMax.x);
                    int y0 = pixelBounds.pMin.y + tile.y * tileSize;
                    int y1 = std::min(y0 + tileSize, pixelBounds.pMax.y);
                    Bounds2i tileBounds(Point2i(x0, y0), Point2i(x1, y1));
                    for (Point2i pPixel : tileBounds) {
                        // Prepare _tileSampler_ for _pPixel_
                        tileSampler->StartPixel(pPixel);
                        tileSampler->SetSampleNumber(iter);

                        // Generate camera ray for pixel for SPPM
                        CameraSample cameraSample =
                            tileSampler->GetCameraSample(pPixel);
                        RayDifferential ray;
                        Spectrum beta =
                            camera->GenerateRayDifferential(cameraSample, &ray);
                        if (beta.IsBlack()) continue;
                        ray.ScaleDifferentials(invSqrtSPP);

                        // Follow camera ray path until a visible point is
                        // created

                        // Get _SPPMPixel_ for _pPixel_
                        Point2i pPixelO = Point2i(pPixel - pixelBounds.pMin);
                        int pixelOffset =
                            pPixelO.x + pPixelO.y * (pixelBounds.pMax.x -
                                                     pixelBounds.pMin.x);
                        SPPMPixel &pixel = pixels[pixelOffset];
                        bool specularBounce = false;
                        for (int depth = 0; depth < maxDepth; ++depth) {
                            SurfaceInteraction isect;
                            ++totalPhotonSurfaceInteractions;
                            if (!scene.Intersect(ray, &isect)) {
                                // Accumulate light contributions for ray with
                                // no intersection
                                for (const auto &light : scene.lights)
                                    pixel.Ld += beta * light->Le(ray);
                                break;
                            }
                            // Process SPPM camera ray intersection

                            // Compute BSDF at SPPM camera ray intersection
                            isect.ComputeScatteringFunctions(ray, arena, true);
                            if (!isect.bsdf) {
                                ray = isect.SpawnRay(ray.d);
                                --depth;
                                continue;
                            }
                            const BSDF &bsdf = *isect.bsdf;

                            // Accumulate direct illumination at SPPM camera ray
                            // intersection
                            Vector3f wo = -ray.d;
                            if (depth == 0 || specularBounce)
                                pixel.Ld += beta * isect.Le(wo);
                            pixel.Ld +=
                                beta * UniformSampleOneLight(
                                           isect, scene, arena, *tileSampler);

                            // Possibly create visible point and end camera path
                            bool isDiffuse =
                                bsdf.NumComponents(
                                    BxDFType(BSDF_DIFFUSE | BSDF_REFLECTION |
                                             BSDF_TRANSMISSION)) > 0;
                            bool isGlossy = bsdf.NumComponents(BxDFType(
                                                BSDF_GLOSSY | BSDF_REFLECTION |
                                                BSDF_TRANSMISSION)) > 0;
                            if (isDiffuse ||
                                (isGlossy && depth == maxDepth - 1)) {
                                pixel.vp = {isect.p, wo, &bsdf, beta};
                                break;
                            }

                            // Spawn ray from SPPM camera path vertex
                            if (depth < maxDepth - 1) {
                                Float pdf;
                                Vector3f wi;
                                BxDFType type;
                                Spectrum f =
                                    bsdf.Sample_f(wo, &wi, tileSampler->Get2D(),
                                                  &pdf, BSDF_ALL, &type);
                                if (pdf == 0. || f.IsBlack()) break;
                                specularBounce = (type & BSDF_SPECULAR) != 0;
                                beta *= f * AbsDot(wi, isect.shading.n) / pdf;
                                if (beta.y() < 0.25) {
                                    Float continueProb =
                                        std::min((Float)1, beta.y());
                                    if (tileSampler->Get1D() > continueProb)
                                        break;
                                    beta /= continueProb;
                                }
                                ray = (RayDifferential)isect.SpawnRay(wi);
                            }
                        }
                    }
                },
                nTiles);
        }
        progress.Update();

        float time = 0;

        auto t1 = std::chrono::high_resolution_clock::now();

        std::vector<SPPMPixel *> OcPixels;
        for (int i = 0; i < nPixels; i++) {
            if (!pixels[i].vp.beta.IsBlack()) {
                OcPixels.push_back(&pixels[i]);
            }
        }
        int nOcPixels = OcPixels.size();

        int threshold = 50;
        int maxD = 7;

        Octree *t = new Octree(&OcPixels, threshold, maxD);
        for (int i = 0; i < nOcPixels; i++) t->addPoint(i);

        // calculate the bounds in the tree
        Bounds3f bounds;
        for (int i = 0; i < nOcPixels; i++) {
            Bounds3f b = OcPixels[i]->WorldBound();
            bounds = Union(bounds, b);
        }

        // if the bounds are too thin in one dimension resize it to be a cube
        if ((bounds.pMax.x - bounds.pMin.x) < initialSearchRadius * 10 ||
            (bounds.pMax.y - bounds.pMin.y) < initialSearchRadius * 10 ||
            (bounds.pMax.z - bounds.pMin.z) < initialSearchRadius * 10) {
            float maxDim = std::max((bounds.pMax.x - bounds.pMin.x),
                                    std::max((bounds.pMax.y - bounds.pMin.y),
                                             (bounds.pMax.z - bounds.pMin.z))) /
                           2;
            Point3f center =
                Point3f(bounds.pMax.x - ((bounds.pMax.x - bounds.pMin.x) / 2),
                        bounds.pMax.y - ((bounds.pMax.y - bounds.pMin.y) / 2),
                        bounds.pMax.z - ((bounds.pMax.z - bounds.pMin.z) / 2));
            bounds.pMin = Point3f(center.x - maxDim, center.y - maxDim,
                                  center.z - maxDim);
            bounds.pMax = Point3f(center.x + maxDim, center.y + maxDim,
                                  center.z + maxDim);
        }

        t->setBounds(bounds);

        t->build();

        auto t2 = std::chrono::high_resolution_clock::now();

        buildtime +=
            (float)std::chrono::duration_cast<std::chrono::microseconds>(t2 -
                                                                         t1)
                .count() /
            1000000;

        auto t3 = std::chrono::high_resolution_clock::now();
        // Trace photons and accumulate contributions
        {
            ProfilePhase _(Prof::SPPMPhotonPass);
            std::vector<MemoryArena> photonShootArenas(MaxThreadIndex());
            ParallelFor(
                [&](int photonIndex) {
                    MemoryArena &arena = photonShootArenas[ThreadIndex];
                    // Follow photon path for _photonIndex_
                    uint64_t haltonIndex =
                        (uint64_t)iter * (uint64_t)photonsPerIteration +
                        photonIndex;
                    int haltonDim = 0;

                    // Choose light to shoot photon from
                    Float lightPdf;
                    Float lightSample =
                        RadicalInverse(haltonDim++, haltonIndex);
                    int lightNum =
                        lightDistr->SampleDiscrete(lightSample, &lightPdf);
                    const std::shared_ptr<Light> &light =
                        scene.lights[lightNum];

                    // Compute sample values for photon ray leaving light source
                    Point2f uLight0(RadicalInverse(haltonDim, haltonIndex),
                                    RadicalInverse(haltonDim + 1, haltonIndex));
                    Point2f uLight1(RadicalInverse(haltonDim + 2, haltonIndex),
                                    RadicalInverse(haltonDim + 3, haltonIndex));
                    Float uLightTime =
                        Lerp(RadicalInverse(haltonDim + 4, haltonIndex),
                             camera->shutterOpen, camera->shutterClose);
                    haltonDim += 5;

                    // Generate _photonRay_ from light source and initialize
                    // _beta_
                    RayDifferential photonRay;
                    Normal3f nLight;
                    Float pdfPos, pdfDir;
                    Spectrum Le =
                        light->Sample_Le(uLight0, uLight1, uLightTime,
                                         &photonRay, &nLight, &pdfPos, &pdfDir);
                    if (pdfPos == 0 || pdfDir == 0 || Le.IsBlack()) return;
                    Spectrum beta = (AbsDot(nLight, photonRay.d) * Le) /
                                    (lightPdf * pdfPos * pdfDir);
                    if (beta.IsBlack()) return;

                    // Follow photon path through scene and record intersections
                    SurfaceInteraction isect;
                    for (int depth = 0; depth < maxDepth; ++depth) {
                        if (!scene.Intersect(photonRay, &isect)) break;
                        ++totalPhotonSurfaceInteractions;
                        if (depth > 0) {
                            const std::vector<int> *pps = t->trace(isect.p);

                            if (pps != nullptr) {
                                for (int i = 0; i < pps->size(); ++i) {
                                    // std::cout << (*pps)[i] << std::endl;
                                    SPPMPixel &pixel = *OcPixels[(*pps)[i]];
                                    // std::cout << "lol11" << std::endl;
                                    ++visiblePointsChecked;
                                    Float radius = pixel.radius;
                                    if (DistanceSquared(pixel.vp.p, isect.p) >
                                        radius * radius)
                                        continue;
                                    // Update _pixel_ $\Phi$ and
                                    // $M$ for nearby photon

                                    Vector3f wi = -photonRay.d;
                                    Spectrum Phi = beta * pixel.vp.bsdf->f(
                                                              pixel.vp.wo, wi);

                                    for (int i = 0; i < Spectrum::nSamples; ++i)
                                        pixel.Phi[i].Add(Phi[i]);
                                    ++pixel.M;
                                }
                            }
                        }
                        // Sample new photon ray direction

                        // Compute BSDF at photon intersection point
                        isect.ComputeScatteringFunctions(
                            photonRay, arena, true, TransportMode::Importance);
                        if (!isect.bsdf) {
                            --depth;
                            photonRay = isect.SpawnRay(photonRay.d);
                            continue;
                        }
                        const BSDF &photonBSDF = *isect.bsdf;

                        // Sample BSDF _fr_ and direction _wi_ for reflected
                        // photon
                        Vector3f wi, wo = -photonRay.d;
                        Float pdf;
                        BxDFType flags;

                        // Generate _bsdfSample_ for outgoing photon sample
                        Point2f bsdfSample(
                            RadicalInverse(haltonDim, haltonIndex),
                            RadicalInverse(haltonDim + 1, haltonIndex));
                        haltonDim += 2;
                        Spectrum fr = photonBSDF.Sample_f(
                            wo, &wi, bsdfSample, &pdf, BSDF_ALL, &flags);
                        if (fr.IsBlack() || pdf == 0.f) break;
                        Spectrum bnew =
                            beta * fr * AbsDot(wi, isect.shading.n) / pdf;

                        // Possibly terminate photon path with Russian roulette
                        Float q = std::max((Float)0, 1 - bnew.y() / beta.y());
                        if (RadicalInverse(haltonDim++, haltonIndex) < q) break;
                        beta = bnew / (1 - q);
                        photonRay = (RayDifferential)isect.SpawnRay(wi);
                    }

                    arena.Reset();
                },
                photonsPerIteration, 8192);
            // 8192

            delete t;
            progress.Update();

            photonPaths += photonsPerIteration;
        }

        auto t4 = std::chrono::high_resolution_clock::now();
        tracetime +=
            (float)std::chrono::duration_cast<std::chrono::microseconds>(t4 -
                                                                         t3)
                .count() /
            1000000;
        // Update pixel values from this pass's photons
        {
            ProfilePhase _(Prof::SPPMStatsUpdate);
            ParallelFor(
                [&](int i) {
                    SPPMPixel &p = pixels[i];
                    if (p.M > 0) {
                        // Update pixel photon count, search radius, and $\tau$
                        // from photons
                        Float gamma = (Float)2 / (Float)3;
                        Float Nnew = p.N + gamma * p.M;
                        Float Rnew = p.radius * std::sqrt(Nnew / (p.N + p.M));
                        Spectrum Phi;
                        for (int j = 0; j < Spectrum::nSamples; ++j)
                            Phi[j] = p.Phi[j];
                        p.tau = (p.tau + p.vp.beta * Phi) * (Rnew * Rnew) /
                                (p.radius * p.radius);
                        p.N = Nnew;
                        p.radius = Rnew;
                        p.M = 0;
                        for (int j = 0; j < Spectrum::nSamples; ++j)
                            p.Phi[j] = (Float)0;
                    }
                    // Reset _VisiblePoint_ in pixel
                    p.vp.beta = 0.;
                    p.vp.bsdf = nullptr;
                },
                nPixels, 4096);
        }

        // Periodically store SPPM image in film and write image
        if (iter + 1 == nIterations || ((iter + 1) % writeFrequency) == 0) {
            int x0 = pixelBounds.pMin.x;
            int x1 = pixelBounds.pMax.x;
            uint64_t Np = (uint64_t)(iter + 1) * (uint64_t)photonsPerIteration;
            std::unique_ptr<Spectrum[]> image(new Spectrum[pixelBounds.Area()]);
            int offset = 0;
            for (int y = pixelBounds.pMin.y; y < pixelBounds.pMax.y; ++y) {
                for (int x = x0; x < x1; ++x) {
                    // Compute radiance _L_ for SPPM pixel _pixel_
                    const SPPMPixel &pixel =
                        pixels[(y - pixelBounds.pMin.y) * (x1 - x0) + (x - x0)];
                    Spectrum L = pixel.Ld / (iter + 1);
                    L += pixel.tau / (Np * Pi * pixel.radius * pixel.radius);
                    image[offset++] = L;
                }
            }
            camera->film->SetImage(image.get());
            camera->film->WriteImage();
            // Write SPPM radius image, if requested
            if (getenv("SPPM_RADIUS")) {
                std::unique_ptr<Float[]> rimg(
                    new Float[3 * pixelBounds.Area()]);
                Float minrad = 1e30f, maxrad = 0;
                for (int y = pixelBounds.pMin.y; y < pixelBounds.pMax.y; ++y) {
                    for (int x = x0; x < x1; ++x) {
                        const SPPMPixel &p =
                            pixels[(y - pixelBounds.pMin.y) * (x1 - x0) +
                                   (x - x0)];
                        minrad = std::min(minrad, p.radius);
                        maxrad = std::max(maxrad, p.radius);
                    }
                }
                fprintf(stderr,
                        "iterations: %d (%.2f s) radius range: %f - %f\n",
                        iter + 1, progress.ElapsedMS() / 1000., minrad, maxrad);
                int offset = 0;
                for (int y = pixelBounds.pMin.y; y < pixelBounds.pMax.y; ++y) {
                    for (int x = x0; x < x1; ++x) {
                        const SPPMPixel &p =
                            pixels[(y - pixelBounds.pMin.y) * (x1 - x0) +
                                   (x - x0)];
                        Float v = 1.f - (p.radius - minrad) / (maxrad - minrad);
                        rimg[offset++] = v;
                        rimg[offset++] = v;
                        rimg[offset++] = v;
                    }
                }
                Point2i res(pixelBounds.pMax.x - pixelBounds.pMin.x,
                            pixelBounds.pMax.y - pixelBounds.pMin.y);
                WriteImage("sppm_radius.png", rimg.get(), pixelBounds, res);
            }
        }

        // Reset memory arenas
        for (int i = 0; i < perThreadArenas.size(); ++i)
            perThreadArenas[i].Reset();
    }
    progress.Done();

    auto t6 = std::chrono::high_resolution_clock::now();

    std::cout << std::endl << "pixels: " << nPixels << std::endl;
    std::cout << "photons per pass: " << photonsPerIteration << std::endl;
    std::cout << "iterations: " << nIterations << std::endl;
    std::cout << "build time: " << buildtime << std::endl;
    std::cout << "trace time: " << tracetime << std::endl;
    std::cout << "render time: "
              << (float)std::chrono::duration_cast<std::chrono::microseconds>(
                     t6 - t5)
                         .count() /
                     1000000
              << std::endl;
    std::cout << "End" << std::endl;
}

void Octree::build() {
    // You know you're a leaf when...
    //
    // 1. The number of points is <= the threshold
    // 2. We've recursed too deep into the tree

    if (pointCount() <= threshold || depth == 0) {
        // Just flag the tree as a leaf
        leaf = true;
        return;
    }

    // create the 8 children
    // Note: The children are coded like this: zyx (in binary).

    for (unsigned int i = 0; i < 8; i++) {
        _child[i] = new Octree(_points, threshold, depth - 1);

        Bounds3f newBounds;
        Vector3f offset;

        switch (i) {
        case 0:
            newBounds = Bounds3f(treeBounds.pMin,
                                 treeBounds.pMin + (treeBounds.Diagonal() / 2));
            break;
        case 1:
            offset =
                Vector3f((treeBounds.pMax.x - treeBounds.pMin.x) / 2, 0, 0);
            newBounds = Bounds3f(
                treeBounds.pMin + offset,
                treeBounds.pMin + (treeBounds.Diagonal() / 2) + offset);
            break;
        case 2:
            offset =
                Vector3f(0, (treeBounds.pMax.y - treeBounds.pMin.y) / 2, 0);
            newBounds = Bounds3f(
                treeBounds.pMin + offset,
                treeBounds.pMin + (treeBounds.Diagonal() / 2) + offset);
            break;
        case 3:
            offset = Vector3f((treeBounds.pMax.x - treeBounds.pMin.x) / 2,
                              (treeBounds.pMax.y - treeBounds.pMin.y) / 2, 0);
            newBounds = Bounds3f(
                treeBounds.pMin + offset,
                treeBounds.pMin + (treeBounds.Diagonal() / 2) + offset);
            break;
        case 4:
            offset =
                Vector3f(0, 0, (treeBounds.pMax.z - treeBounds.pMin.z) / 2);
            newBounds = Bounds3f(
                treeBounds.pMin + offset,
                treeBounds.pMin + (treeBounds.Diagonal() / 2) + offset);
            break;
        case 5:
            offset = Vector3f((treeBounds.pMax.x - treeBounds.pMin.x) / 2, 0,
                              (treeBounds.pMax.z - treeBounds.pMin.z) / 2);
            newBounds = Bounds3f(
                treeBounds.pMin + offset,
                treeBounds.pMin + (treeBounds.Diagonal() / 2) + offset);
            break;
        case 6:
            offset = Vector3f(0, (treeBounds.pMax.y - treeBounds.pMin.y) / 2,
                              (treeBounds.pMax.z - treeBounds.pMin.z) / 2);
            newBounds = Bounds3f(
                treeBounds.pMin + offset,
                treeBounds.pMin + (treeBounds.Diagonal() / 2) + offset);
            break;
        case 7:
            newBounds = Bounds3f(treeBounds.pMin + (treeBounds.Diagonal() / 2),
                                 treeBounds.pMax);
            break;
        }

        _child[i]->treeBounds = newBounds;
    }

    // Center of this node
    c = Point3f((treeBounds.pMax.x + treeBounds.pMin.x) / 2,
                (treeBounds.pMax.y + treeBounds.pMin.y) / 2,
                (treeBounds.pMax.z + treeBounds.pMin.z) / 2);

    // Classify each point to a child node using the encoding mentioned up.

    for (unsigned int i = 0; i < pointCount(); i++) {
        SPPMPixel *&p = (*_points)[assigned_points[i]];
        Bounds3f b = p->WorldBound();

        if (b.pMin.x < c.x) {
            if (b.pMin.y < c.y) {
                if (b.pMin.z < c.z) _child[0]->addPoint(assigned_points[i]);
                if (b.pMax.z > c.z) _child[4]->addPoint(assigned_points[i]);
            }
            if (b.pMax.y > c.y) {
                if (b.pMin.z < c.z) _child[2]->addPoint(assigned_points[i]);
                if (b.pMax.z > c.z) _child[6]->addPoint(assigned_points[i]);
            }
        }

        if (b.pMax.x > c.x) {
            if (b.pMin.y < c.y) {
                if (b.pMin.z < c.z) _child[1]->addPoint(assigned_points[i]);
                if (b.pMax.z > c.z) _child[5]->addPoint(assigned_points[i]);
            }
            if (b.pMax.y > c.y) {
                if (b.pMin.z < c.z) _child[3]->addPoint(assigned_points[i]);
                if (b.pMax.z > c.z) _child[7]->addPoint(assigned_points[i]);
            }
        }
    }

    // build the children
    for (unsigned int i = 0; i < 8; i++) {
        _child[i]->build();
    }
}

std::vector<int> *Octree::trace(Point3f p) {
    if (leaf)
        return &assigned_points;
    else {
        Octree *t;
        if (p.x < c.x) {
            if (p.y < c.y) {
                if (p.z < c.z)
                    t = _child[0];
                else
                    t = _child[4];
            } else {
                if (p.z < c.z)
                    t = _child[2];
                else
                    t = _child[6];
            }

        } else {
            if (p.y < c.y) {
                if (p.z < c.z)
                    t = _child[1];
                else
                    t = _child[5];
            } else {
                if (p.z < c.z)
                    t = _child[3];
                else
                    t = _child[7];
            }
        }
        if (t != nullptr) {
            return t->trace(p);
        } else {
            return nullptr;
        }
    }
}

Integrator *CreateOctreeSPPMIntegrator(
    const ParamSet &params, std::shared_ptr<const Camera> camera) {
    int nIterations =
        params.FindOneInt("iterations", params.FindOneInt("numiterations", 64));
    int maxDepth = params.FindOneInt("maxdepth", 5);
    int photonsPerIter = params.FindOneInt("photonsperiteration", -1);
    int writeFreq = params.FindOneInt("imagewritefrequency", 1 << 31);
    Float radius = params.FindOneFloat("radius", 1.f);
    if (PbrtOptions.quickRender) nIterations = std::max(1, nIterations / 16);
    return new OctreeSPPMIntegrator(camera, nIterations, photonsPerIter,
                                            maxDepth, radius, writeFreq);
}

}  // namespace pbrt
