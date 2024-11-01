#pragma once

#include "api.h"

extern "C" {

struct Vector3 {
    float x, y, z;
};

struct Ray {
    Vector3 origin;
    Vector3 direction;
    float minDistance;
};

#define INVALID_MESH_ID ((unsigned int) -1)

struct Hit {
    unsigned int meshId;
    unsigned int primId;
    float u, v;
    float distance;
};

TINY_EMBREE_API void* InitScene();

TINY_EMBREE_API int AddTriangleMesh(void* scene, const float* vertices, int numVerts,
                                    const int* indices, int numIdx);

TINY_EMBREE_API void FinalizeScene(void* scene);

TINY_EMBREE_API void TraceSingle(void* scene, const Ray* ray, Hit* hit);
TINY_EMBREE_API bool IsOccluded(void* scene, const Ray* ray, float maxDistance);

TINY_EMBREE_API void DeleteScene(void* scene);

}