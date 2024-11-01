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

#ifndef _KDTREENODE_INPLACE_H_
#define _KDTREENODE_INPLACE_H_

#include <tbb/concurrent_vector.h>

#include "SPPM_Integrators/SAH_InPlace_KD_par.h"
#include "BoxEdge_inplace.h"
#include "common_inplace.h"


using namespace pbrt;
using namespace std;


class KdTreeNode_inplace {
public:
    
   // edge that creates splitting plane
   BoxEdge_inplace *splitEdge;
  
  Bounds3f extent;

  // interior node variables
  KdTreeNode_inplace *left;
  KdTreeNode_inplace *right;

  // leaf node variables
  std::vector<int> *triangleIndices;

  // count of triangles in the subtree rooted by this node
  // -- needed since we no longer move boxEdge/s around
  unsigned int triangleCount;

  tbb::concurrent_vector<int> *cc_triangleIndices;

  KdTreeNode_inplace()
      : triangleCount(0),
        cc_triangleIndices(NULL),
        left(NULL),
        right(NULL),
        splitEdge(NULL),
        triangleIndices(NULL),
        custom_mm(true) {  }

  ~KdTreeNode_inplace() {
      if (custom_mm) {
          // KdTreeNode_tbb2 are handled by custom bump-pointer allocator
          return;
      }

      if (left) {
          delete left;
          left = NULL;
      }

      if (right) {
          delete right;
          right = NULL;
      }

      if (splitEdge) {
          delete splitEdge;
          splitEdge = NULL;
      }

      if (triangleIndices) {
          delete triangleIndices;
          triangleIndices = NULL;
      }
  }

  protected:
  
  bool custom_mm;
};

#endif // _KDTREENODE_INPLACE_H_
