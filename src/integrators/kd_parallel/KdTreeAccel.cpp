/*
   Copyright (c) 2010 University of Illinois
   All rights reserved.

   Developed by:           DeNovo group, Graphis@Illinois
                           University of Illinois
                           http://denovo.cs.illinois.edu
                           http://graphics.cs.illinois.edu

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the
   "Software"), to deal with the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimers.

    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following disclaimers
      in the documentation and/or other materials provided with the
      distribution.

    * Neither the names of DeNovo group, Graphics@Illinois, 
      University of Illinois, nor the names of its contributors may be used to 
      endorse or promote products derived from this Software without specific 
      prior written permission.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR
   ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
   TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
   SOFTWARE OR THE USE OR OTHER DEALINGS WITH THE SOFTWARE.
*/

#include <iomanip>
#include <stdio.h>

#include <tbb/task_scheduler_init.h>
#include <tbb/partitioner.h>
#include <tbb/parallel_for.h>
#include <tbb/parallel_sort.h>

#include "KdTreeAccel.h"
#include "CreateEdges_task.h"
#include "SetupTriangles_task.h"
#include "PrescanTab.h"
#include "FindBestPlane_AoS_prescan_task.h"
#include "FindBestPlane_AoS_task.h"
#include "Split_task.h"

using namespace std;
using namespace tbb;
using namespace pbrt;



bool ver_splitedge;

KdTreeAccel::KdTreeAccel(std::vector<SPPMPixel *> kdPixels, 
                         uint numThreads,
                         uint maxDepth, uint maxObjInNode)
    : kdPixels(kdPixels),
      m_numThreads(numThreads),
      m_maxDepth(maxDepth),
      m_maxObjInNode(maxObjInNode) {}

void KdTreeAccel::build() {
    
  // init task scheduler
    task_scheduler_init init(m_numThreads);
  
  uint n = kdPixels.size(); // number of triangles
  
  // x, y, z edges concatenated
  xbegin_idx = 0;
  xend_idx = xbegin_idx + 2*n;
  ybegin_idx = xend_idx;
  yend_idx = ybegin_idx + 2*n;
  zbegin_idx = yend_idx;
  zend_idx = zbegin_idx + 2*n;
  v_Triangle_aux &tris = *new v_Triangle_aux(n);
  
  // build consecutive array of kdTreeNode/s -- assume full-tree

  kdTreeNodeObj = new v_KdTreeNode_inplace(1 << (m_maxDepth + 1));

  // root_ is the first one
  
  root_ = &((*kdTreeNodeObj)[0]);
  
  // all triangles
  root_->triangleCount = n;


  root_->triangleIndices = new vector<int>();

  proxy.resize(6*n); // sort proxy.. sort this to find out right index
                            // for boxEdge/s (unpacked) -- index into tab/s
  
  parallel_for(blocked_range<size_t>(xbegin_idx, xbegin_idx+n),
               CreateEdges_task(proxy, tris, root_, kdPixels, 0, 0),
               auto_partitioner());
  parallel_for(blocked_range<size_t>(ybegin_idx, ybegin_idx+n),
               CreateEdges_task(proxy, tris, root_, kdPixels, 1, 2 * n),
               auto_partitioner());
  parallel_for(blocked_range<size_t>(zbegin_idx, zbegin_idx+n),
               CreateEdges_task(proxy, tris, root_, kdPixels, 2, 4 * n),
               auto_partitioner());

  tbb::parallel_sort(proxy.begin(), proxy.begin() + 2 * n, std::less<BoxEdge_inplace>());
  tbb::parallel_sort(proxy.begin()+2*n, proxy.begin() + 4 * n, std::less<BoxEdge_inplace>());
  tbb::parallel_sort(proxy.begin()+4*n, proxy.begin() + 6 * n, std::less<BoxEdge_inplace>());

  parallel_for(blocked_range<size_t>(xbegin_idx, xend_idx),
               SetupTriangles_task(proxy), auto_partitioner());
  parallel_for(blocked_range<size_t>(ybegin_idx, yend_idx),
               SetupTriangles_task(proxy), auto_partitioner());
  parallel_for(blocked_range<size_t>(zbegin_idx, zend_idx),
               SetupTriangles_task(proxy), auto_partitioner());

  //set the bounds of the whole tree using the sorted list of boxedges
  Bounds3f bounds = Bounds3f();
  bounds.pMin = Point3f(proxy[0].t, proxy[2 * n].t, proxy[4 * n].t);
  bounds.pMax = Point3f(proxy[(2 * n) - 1].t, proxy[(4 * n) - 1].t,
                        proxy[(6 * n) - 1].t);
  root_->extent = bounds;


  pRootTask = new(task::allocate_root()) empty_task;
    
  parallel_build(proxy, tris, m_maxDepth,
                 m_maxObjInNode); 
  
}

KdTreeNode_inplace *KdTreeAccel::root() { return root_; }

