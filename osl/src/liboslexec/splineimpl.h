/*
Copyright (c) 2009-2010 Sony Pictures Imageworks Inc., et al.
All Rights Reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
* Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.
* Neither the name of Sony Pictures Imageworks nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef SPLINEIMPL_H
#define SPLINEIMPL_H

// avoid naming conflict with MSVC macro
#ifdef BTYPE
#undef BTYPE
#endif

#ifdef OSL_NAMESPACE
namespace OSL_NAMESPACE {
#endif
namespace OSL {

namespace pvt {

// declaring a Spline namespace to avoid polluting the existing
// namespaces with all these templated helper functions.
namespace Spline {


// We need to know explicitly whether the knots have
// derivatives associated with them because of the way
// Dual2<T> forms of arrays are stored..  Arrays with 
// derivatives are stored:
//   T T T... T.dx T.dx T.dx... T.dy T.dy T.dy...
// This means, we need to explicitly construct the Dual2<T>
// form of the knots on the fly.
// if 'is_dual' == true, then OUTTYPE == Dual2<INTYPE>
// if 'is_dual' == false, then OUTTYPE == INTYPE

// This functor will extract a T or a Dual2<T> type from a VaryingRef array
template <class OUTTYPE, class INTYPE, bool is_dual> struct extractValueFromArray
{
    OUTTYPE operator()(INTYPE *value, int array_length, int idx);
};

template <class OUTTYPE, class INTYPE>
struct extractValueFromArray<OUTTYPE, INTYPE, true> 
{
    OUTTYPE operator()(INTYPE *value, int array_length, int idx)
    {
        return OUTTYPE( value[idx + 0*array_length], 
                        value[idx + 1*array_length],
                        value[idx + 2*array_length] );
    }
};

template <class OUTTYPE, class INTYPE>
struct extractValueFromArray<OUTTYPE, INTYPE, false> 
{
    OUTTYPE operator()(INTYPE *value, int array_length, int idx)
    {
        return OUTTYPE( value[idx] );
    }
};

inline Dual2<float> Clamp(Dual2<float> x, Dual2<float> minv, Dual2<float> maxv)
{
    return dual_clamp(x, minv, maxv);
}

inline float Clamp(float x, float minv, float maxv) {
    if (x < minv) return minv;
    else if (x > maxv) return maxv;
    else return x;
};



// Eliminate the derivatives of a number
template<class T> inline T removeDerivatives (const T x)         { return x;       }
template<class T> inline T removeDerivatives (const Dual2<T> &x) { return x.val(); }

// Simple templated "copy" function
template <class T> inline void assignment(T &a, T &b)        { a = b;       }
template <class T> inline void assignment(T &a, Dual2<T> &b) { a = b.val(); }



struct SplineBasis {
   ustring  basis_name;
   int      basis_step;
   Matrix44 basis;
};

const SplineBasis *getSplineBasis(const ustring &basis_name);

template <class ATYPE, class BTYPE, class CTYPE, class DTYPE, bool knot_derivs >
void spline_evaluate(const SplineBasis *spline, 
                     ATYPE &result, 
                     BTYPE &xval, 
                     DTYPE *knots,
                     int knot_count)
{
    BTYPE x = Clamp(xval, BTYPE(0.0f), BTYPE(1.0f));
    int nsegs = ((knot_count - 4) / spline->basis_step) + 1;
    x = x*(float)nsegs;
    float seg_x = removeDerivatives(x);
    int segnum = (int)seg_x;
    if (segnum > (nsegs-1))
       segnum = nsegs-1;
    // x is the position along segment 'segnum'
    x = x - float(segnum);
    int s = segnum*spline->basis_step;
    int len = knot_count;

    // create a functor so we can cleanly(!) extract
    // the knot elements
    extractValueFromArray<CTYPE, DTYPE, knot_derivs> myExtract;
    CTYPE P[4];
    for (int k = 0; k < 4; k++) {
        P[k] = myExtract(knots, len, s + k);
    }

    CTYPE tk[4];
    for (int k = 0; k < 4; k++) {
        tk[k] = spline->basis[k][0] * P[0] +
                spline->basis[k][1] * P[1] +
                spline->basis[k][2] * P[2] + 
                spline->basis[k][3] * P[3];
    }

    ATYPE tresult;
    // The following is what we want, but this gives me template errors
    // which I'm too lazy to decipher:
    //    tresult = ((tk[0]*x + tk[1])*x + tk[2])*x + tk[3];
    tresult = (tk[0]   * x + tk[1]);
    tresult = (tresult * x + tk[2]);
    tresult = (tresult * x + tk[3]);
    assignment(result, tresult);
}



}; // namespace Spline
}; // namespace pvt
}; // namespace OSL
#ifdef OSL_NAMESPACE
}; // end namespace OSL_NAMESPACE
#endif

#endif // SPLINEIMPL_H
