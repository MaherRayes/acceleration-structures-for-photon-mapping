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

#ifndef _BOXEDGE_INPLACE_H_
#define _BOXEDGE_INPLACE_H_


 // forward declaration - circular dependency
class Triangle_aux;

// proxy object for sorting + building relationships
// pack as much as possible for maximum spatial-locality
class BoxEdge_inplace {
public:
  // inherited
//   float t;
//   uint triangleIndex;
//   bool edgeType;
//   char axis;

  Triangle_aux *tri;

  BoxEdge_inplace() : t(std::numeric_limits<float>::max()), triangleIndex(0), edgeType(0), axis(0), tri(NULL) {}
  BoxEdge_inplace(float t, unsigned int tri_idx, bool type, char axis)
      : t(t),
        triangleIndex(tri_idx),
        edgeType(type),
        axis(axis),
        tri(NULL) {}


  float t;
  unsigned int triangleIndex;
  bool edgeType;
  char axis;

  bool operator<(const BoxEdge_inplace &RHS) const {
      if (this->t == RHS.t) {
          if (this->triangleIndex != RHS.triangleIndex) {
              return this->triangleIndex < RHS.triangleIndex;
          } else {  // at this point we have two edges with equal t and index
              // resolve by comparing edge type
              return (int)(this->edgeType) < (int)(RHS.edgeType);
          }
      } else {
          return this->t < RHS.t;
      }
  }

  friend std::ostream &operator<<(std::ostream &o, const BoxEdge_inplace &edge);
  friend std::ostream &operator<<(std::ostream &o, const BoxEdge_inplace *edge);
};
#endif // _BOXEDGE_INPLACE_H_
