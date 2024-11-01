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

#ifndef _KDTREEACCEL_H_
#define _KDTREEACCEL_H_

#include <tbb/task.h>

#include "common_inplace.h"
#include "SplitMemo.h"
#include "SAH.h"
#include "KdTreeNode_inplace.h"
#include "integrators/kdsppm.h"

using namespace pbrt;

class KdTreeAccel {
 public:
    KdTreeAccel(std::vector<SPPMPixel *> kdPixels, uint numThreads,
               uint max_depth, uint maxObjInNode);

  ~KdTreeAccel() {
//     if (kdTreeNodeObj) delete kdTreeNodeObj;
//     kdTreeNodeObj = NULL;
    kdTreeNodeObj->clear();
  }

  const SAH sah;
  // Mandatory functions
  void build();

  KdTreeNode_inplace* root();

protected:
  KdTreeNode_inplace *m_root;
  uint m_numThreads;
  uint m_maxDepth;
  uint m_maxObjInNode;
  const std::vector<SPPMPixel *> kdPixels;
  

private:
  v_KdTreeNode_inplace *kdTreeNodeObj;
  KdTreeNode_inplace *root_;
  size_t xbegin_idx, xend_idx, ybegin_idx, yend_idx, zbegin_idx, zend_idx;

  // root task used by _ll functions below
  tbb::task *pRootTask;

  // helper functions
  void parallel_build(v_BoxEdge_inplace &boxEdges, v_Triangle_aux &tris,
                      uint maxDepth, uint maxObjInNode);

  void findBestPlane(v_BoxEdge_inplace &boxEdges, v_Triangle_aux &tris,
                     vp_KdTreeNode_inplace *live, SplitMemo *memo);
  void classifyTriangles(v_Triangle_aux &tris, vp_KdTreeNode_inplace *live, 
                         SplitMemo *memo, KdTreeNode_inplace *base);
  void fill(v_Triangle_aux &tris, vp_KdTreeNode_inplace *live);
  void moveTriangles(KdTreeNode_inplace *node);

  int begin_idx[3], end_idx[3];

  v_BoxEdge_inplace proxy;

};

#endif // _KDTREEACCEL_H_
