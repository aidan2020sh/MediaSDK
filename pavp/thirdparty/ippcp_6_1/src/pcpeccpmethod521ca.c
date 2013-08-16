/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2004-2007 Intel Corporation. All Rights Reserved.
//
//
// Purpose:
//    Cryptography Primitive.
//    EC methods over GF(P521)
//
// Contents:
//    ECCP521_SetPointProjective()
//    ECCP521_SetPointAffine()
//    ECCP521_GetPointProjective()
//
//    ECCP521_GetPointProjective()
//    ECCP521_GetPointAffine()
//
//    ECCP521_IsPointOnCurve()
//
//    ECCP521_ComparePoint()
//    ECCP521_NegPoint()
//    ECCP521_DblPoint()
//    ECCP521_AddPoint()
//    ECCP521_MulPoint()
//    ECCP521_ProdPoint()
//
//
//    Created: Thu 12-Aug-2004 17:12
//  Author(s): Sergey Kirillov
*/
#if defined(_IPP_v50_)

#include "precomp.h"
#include "owncp.h"
#include "pcpeccppoint.h"
#include "pcpeccpmethod.h"
#include "pcpeccpmethodcom.h"
#include "pcpeccpmethod521.h"
#include "pcppma521.h"

#if !defined(_IPP_TRS)

static
ECCP_METHOD ECCP521 = {
   //ECCP_CopyPoint,

   ECCP521_SetPointProjective,
   ECCP521_SetPointAffine,

   ECCP521_GetPointProjective,
   ECCP521_GetPointAffine,

   //ECCP_SetPointToInfinity,
   //ECCP_SetPointToAffineInfinity0,
   //ECCP_SetPointToAffineInfinity1,

   //ECCP_IsPointAtInfinity,
   //ECCP_IsPointAtAffineInfinity0,
   //ECCP_IsPointAtAffineInfinity1,
   ECCP521_IsPointOnCurve,

   ECCP521_ComparePoint,
   ECCP521_NegPoint,
   ECCP521_DblPoint,
   ECCP521_AddPoint,
   ECCP521_MulPoint,
   ECCP521_ProdPoint
};


/*
// Returns reference
*/
ECCP_METHOD* ECCP521_Methods(void)
{
   return &ECCP521;
}

/*
// ECCP521_PoinSettProjective
// Converts regular projective triplet (pX,pY,pZ) into pPoint
*/
void ECCP521_SetPointProjective(const IppsBigNumState* pX,
                             const IppsBigNumState* pY,
                             const IppsBigNumState* pZ,
                             IppsECCPPointState* pPoint,
                             const IppsECCPState* pECC)
{
   #if defined(_IPP_TRS)
   DECLARE_BN_Word(_trs_one32_, 1);
   INIT_BN_Word(_trs_one32_);
   #endif

   UNREFERENCED_PARAMETER(pECC);
   cpBN_copy(pX, ECP_POINT_X(pPoint));
   cpBN_copy(pY, ECP_POINT_Y(pPoint));
   cpBN_copy(pZ, ECP_POINT_Z(pPoint));
   ECP_POINT_AFFINE(pPoint) = Cmp_BN(pZ, BN_ONE_REF())==0;
}

/*
// ECCP521_PointAffineSet
// Converts regular affine pair (pX,pY) into pPoint
*/
void ECCP521_SetPointAffine(const IppsBigNumState* pX,
                         const IppsBigNumState* pY,
                         IppsECCPPointState* pPoint,
                         const IppsECCPState* pECC)
{
   #if defined(_IPP_TRS)
   DECLARE_BN_Word(_trs_one32_, 1);
   INIT_BN_Word(_trs_one32_);
   #endif

   ECCP521_SetPointProjective(pX, pY, BN_ONE_REF(),
                              pPoint, pECC);
}

/*
// ECCP521_GetPointProjective
// Converts pPoint into regular projective triplet (pX,pY,pZ)
*/
void ECCP521_GetPointProjective(IppsBigNumState* pX,
                             IppsBigNumState* pY,
                             IppsBigNumState* pZ,
                             const IppsECCPPointState* pPoint,
                             const IppsECCPState* pECC)
{
   UNREFERENCED_PARAMETER(pECC);
   cpBN_copy(ECP_POINT_X(pPoint), pX);
   cpBN_copy(ECP_POINT_Y(pPoint), pY);
   cpBN_copy(ECP_POINT_Z(pPoint), pZ);
}

/*
// ECCP521_GetPointAffine
//
// Converts pPoint into regular affine pair (pX,pY)
//
// Note:
// pPoint is not point at Infinity
// transform  (X, Y, Z)  into  (x, y) = (X/Z^2, Y/Z^3)
*/
void ECCP521_GetPointAffine(IppsBigNumState* pX, IppsBigNumState* pY,
                         const IppsECCPPointState* pPoint,
                         const IppsECCPState* pECC,
                         BigNumNode* pList)
{
   /* case Z == 1 */
   if( ECP_POINT_AFFINE(pPoint) ) {
      if(pX) {
         cpBN_copy(ECP_POINT_X(pPoint), pX);
      }
      if(pY) {
         cpBN_copy(ECP_POINT_Y(pPoint), pY);
      }
   }

   /* case Z != 1 */
   else {
      IppsBigNumState* pT = cpBigNumListGet(&pList);
      IppsBigNumState* pU = cpBigNumListGet(&pList);
      IppsBigNumState* pModulo = MNT_MODULO( ECP_PMONT(pECC) );

      /* U = 1/Z */
      PMA521_inv(pU, ECP_POINT_Z(pPoint), pModulo);
      /* T = 1/(Z^2) */
      PMA521_sqr(pT, pU);

      if(pX) {
         PMA521_mul(pX,pT, ECP_POINT_X(pPoint));
      }
      if(pY) {
         /* U = 1/(Z^3) */
         PMA521_mul(pU, pU, pT);
         PMA521_mul(pY,pU, ECP_POINT_Y(pPoint));
      }
   }
}

/*
// ECCP521_IsPointOnCurve
//
// Test point is lie on curve
//
// Note
// We deal with equation: y^2 = x^3 + A*x + B.
// Or in projective coordinates: Y^2 = X^3 + a*X*Z^4 + b*Z^6.
// The point under test is given by projective triplet (X,Y,Z),
// which represents actually (x,y) = (X/Z^2,Y/Z^3).
*/
int ECCP521_IsPointOnCurve(const IppsECCPPointState* pPoint,
                        const IppsECCPState* pECC,
                        BigNumNode* pList)
{
   /* let think Infinity point is on the curve */
   if( ECCP_IsPointAtInfinity(pPoint) )
      return 1;

   else {
      IppsBigNumState* pR = cpBigNumListGet(&pList);
      IppsBigNumState* pT = cpBigNumListGet(&pList);

      PMA521_sqr(pR, ECP_POINT_X(pPoint));      // R = X^3
      PMA521_mul(pR, pR, ECP_POINT_X(pPoint));

      /* case Z != 1 */
      if( !ECP_POINT_AFFINE(pPoint) ) {
         IppsBigNumState* pZ4 = cpBigNumListGet(&pList);
         IppsBigNumState* pZ6 = cpBigNumListGet(&pList);
         PMA521_sqr(pT,  ECP_POINT_Z(pPoint));    // Z^2
         PMA521_sqr(pZ4, pT);                     // Z^4
         PMA521_mul(pZ6, pZ4, pT);                // Z^6

         PMA521_mul(pT, pZ4, ECP_POINT_X(pPoint)); // T = X*Z^4
         if( ECP_AMI3(pECC) ) {
            IppsBigNumState* pU = cpBigNumListGet(&pList);
               PMA521_add(pU, pT, pT);               // R = X^3 +a*X*Z^4
               PMA521_add(pU, pU, pT);
               PMA521_sub(pR, pR, pU);
         }
         else {
            PMA521_mul(pT, pT, ECP_A(pECC));     // R = X^3 +a*X*Z^4
            PMA521_add(pR, pR, pT);
         }
           PMA521_mul(pT, pZ6, ECP_B(pECC));       // R = X^3 +a*X*Z^4 + b*Z^6
           PMA521_add(pR, pR, pT);

      }
      /* case Z == 1 */
      else {
         if( ECP_AMI3(pECC) ) {
               PMA521_add(pT, ECP_POINT_X(pPoint), ECP_POINT_X(pPoint)); // R = X^3 +a*X
               PMA521_add(pT, pT, ECP_POINT_X(pPoint));
               PMA521_sub(pR, pR, pT);
         }
         else {
               PMA521_mul(pT, ECP_POINT_X(pPoint), ECP_A(pECC));       // R = X^3 +a*X
               PMA521_add(pR, pR, pT);
         }
         PMA521_add(pR, pR, ECP_B(pECC));                   // R = X^3 +a*X + b
      }
      PMA521_sqr(pT, ECP_POINT_Y(pPoint));  // T = Y^2
      return 0==Cmp_BN(pR, pT);
   }
}

/*
// ECCP521_ComparePoint
//
// Compare two points:
//    returns 0 => pP==pQ (maybe both pP and pQ are at Infinity)
//    returns 1 => pP!=pQ
//
// Note
// In general we chech:
//    P_X*Q_Z^2 ~ Q_X*P_Z^2
//    P_Y*Q_Z^3 ~ Q_Y*P_Z^3
*/
int ECCP521_ComparePoint(const IppsECCPPointState* pP,
                      const IppsECCPPointState* pQ,
                      const IppsECCPState* pECC,
                      BigNumNode* pList)
{
   UNREFERENCED_PARAMETER(pECC);

   /* P or/and Q at Infinity */
   if( ECCP_IsPointAtInfinity(pP) )
      return ECCP_IsPointAtInfinity(pQ)? 0:1;
   if( ECCP_IsPointAtInfinity(pQ) )
      return ECCP_IsPointAtInfinity(pP)? 0:1;

   /* (P_Z==1) && (Q_Z==1) */
    if( ECP_POINT_AFFINE(pP) && ECP_POINT_AFFINE(pQ) )
      return ((0==Cmp_BN(ECP_POINT_X(pP),ECP_POINT_X(pQ))) && (0==Cmp_BN(ECP_POINT_Y(pP),ECP_POINT_Y(pQ))))? 0:1;

   {
      IppsBigNumState* pPtmp = cpBigNumListGet(&pList);
      IppsBigNumState* pQtmp = cpBigNumListGet(&pList);
      IppsBigNumState* pPZ   = cpBigNumListGet(&pList);
      IppsBigNumState* pQZ   = cpBigNumListGet(&pList);

      /* P_X*Q_Z^2 ~ Q_X*P_Z^2 */
      if( !ECP_POINT_AFFINE(pQ) ) {
         PMA521_sqr(pQZ, ECP_POINT_Z(pQ));      /* Ptmp = P_X*Q_Z^2 */
         PMA521_mul(pPtmp, ECP_POINT_X(pP), pQZ);
      }
      else {
         PMA_set(pPtmp, ECP_POINT_X(pP));
      }
      if( !ECP_POINT_AFFINE(pP) ) {
         PMA521_sqr(pPZ, ECP_POINT_Z(pP));      /* Qtmp = Q_X*P_Z^2 */
         PMA521_mul(pQtmp, ECP_POINT_X(pQ), pPZ);
      }
      else {
         PMA_set(pQtmp, ECP_POINT_X(pQ));
      }
      if ( Cmp_BN(pPtmp, pQtmp) )
         return 1;   /* poionts are different: (P_X*Q_Z^2) != (Q_X*P_Z^2) */

      /* P_Y*Q_Z^3 ~ Q_Y*P_Z^3 */
      if( !ECP_POINT_AFFINE(pQ) ) {
         PMA521_mul(pQZ, pQZ, ECP_POINT_Z(pQ)); /* Ptmp = P_Y*Q_Z^3 */
         PMA521_mul(pPtmp, ECP_POINT_Y(pP), pQZ);
      }
      else {
         PMA_set(pPtmp, ECP_POINT_Y(pP));
      }
      if( !ECP_POINT_AFFINE(pP) ) {
         PMA521_mul(pPZ, pPZ, ECP_POINT_Z(pP)); /* Qtmp = Q_Y*P_Z^3 */
         PMA521_mul(pQtmp, ECP_POINT_Y(pQ), pPZ);
      }
      else {
         PMA_set(pQtmp, ECP_POINT_Y(pQ));
      }
      return Cmp_BN(pPtmp, pQtmp)? 1:0;
   }
}

/*
// ECCP521_NegPoint
//
// Negative point
*/
void ECCP521_NegPoint(const IppsECCPPointState* pP,
                   IppsECCPPointState* pR,
                   const IppsECCPState* pECC)
{
   UNREFERENCED_PARAMETER(pECC);

   /* test point at Infinity */
   if( ECCP_IsPointAtInfinity(pP) )
      ECCP_SetPointToInfinity(pR);

   else {
      Ipp32u* pRy = BN_NUMBER(ECP_POINT_Y(pR));
      Ipp32u* pPy = BN_NUMBER(ECP_POINT_Y(pP));
      int size = LEN_P521;

      if( pP!=pR ) {
         PMA_set(ECP_POINT_X(pR), ECP_POINT_X(pP));
         PMA_set(ECP_POINT_Z(pR), ECP_POINT_Z(pP));
         ECP_POINT_AFFINE(pR) = ECP_POINT_AFFINE(pP);
      }
      FE_SUB(pRy, secp521r1_p, pPy, LEN_P521);
      FIX_BNU(pRy,size);
      BN_SIZE(ECP_POINT_Y(pR)) = size;
      BN_SIGN(ECP_POINT_Y(pR)) = IppsBigNumPOS;
   }
}

/*
// ECCP521_DblPoint
//
// Double point
*/
void ECCP521_DblPoint(const IppsECCPPointState* pP,
                   IppsECCPPointState* pR,
                   const IppsECCPState* pECC,
                   BigNumNode* pList)
{
   /* P at infinity */
   if( ECCP_IsPointAtInfinity(pP) )
      ECCP_SetPointToInfinity(pR);

   else {
      IppsBigNumState* bnV = cpBigNumListGet(&pList);
      IppsBigNumState* bnU = cpBigNumListGet(&pList);
      IppsBigNumState* bnM = cpBigNumListGet(&pList);
      IppsBigNumState* bnS = cpBigNumListGet(&pList);
      IppsBigNumState* bnT = cpBigNumListGet(&pList);

      /* M = 3*X^2 + A*Z^4 */
       if( ECP_POINT_AFFINE(pP) ) {
           PMA521_sqr(bnU, ECP_POINT_X(pP));
           PMA521_add(bnM, bnU, bnU);
           PMA521_add(bnM, bnM, bnU);
           PMA521_add(bnM, bnM, ECP_A(pECC));
        }
       else if( ECP_AMI3(pECC) ) {
           PMA521_sqr(bnU, ECP_POINT_Z(pP));
           PMA521_add(bnS, ECP_POINT_X(pP), bnU);
           PMA521_sub(bnT, ECP_POINT_X(pP), bnU);
           PMA521_mul(bnM, bnS, bnT);
           PMA521_add(bnU, bnM, bnM);
           PMA521_add(bnM, bnU, bnM);
        }
       else {
           PMA521_sqr(bnU, ECP_POINT_X(pP));
           PMA521_add(bnM, bnU, bnU);
           PMA521_add(bnM, bnM, bnU);
           PMA521_sqr(bnU, ECP_POINT_Z(pP));
           PMA521_sqr(bnU, bnU);
           PMA521_mul(bnU, bnU, ECP_A(pECC));
           PMA521_add(bnM, bnM, bnU);
        }

      PMA521_add(bnV, ECP_POINT_Y(pP), ECP_POINT_Y(pP));

      /* R_Z = 2*Y*Z */
      if( ECP_POINT_AFFINE(pP) ) {
         PMA_set(ECP_POINT_Z(pR), bnV);
      }
      else {
         PMA521_mul(ECP_POINT_Z(pR), bnV, ECP_POINT_Z(pP));
      }

      /* S = 4*X*Y^2 */
      PMA521_sqr(bnT, bnV);
      PMA521_mul(bnS, bnT, ECP_POINT_X(pP));

      /* R_X = M^2 - 2*S */
      PMA521_sqr(bnU, bnM);
      PMA521_sub(bnU, bnU, bnS);
      PMA521_sub(ECP_POINT_X(pR), bnU, bnS);

      /* T = 8*Y^4 */
      PMA521_mul(bnV, bnV, ECP_POINT_Y(pP));
      PMA521_mul(bnT, bnT, bnV);

      /* R_Y = M*(S - R_X) - T */
      PMA521_sub(bnS, bnS, ECP_POINT_X(pR));
      PMA521_mul(bnS, bnS, bnM);
      PMA521_sub(ECP_POINT_Y(pR), bnS, bnT);

      ECP_POINT_AFFINE(pR) = 0;
   }
}

/*
// ECCP521_AddPoint
//
// Add points
*/
void ECCP521_AddPoint(const IppsECCPPointState* pP,
                   const IppsECCPPointState* pQ,
                   IppsECCPPointState* pR,
                   const IppsECCPState* pECC,
                   BigNumNode* pList)
{
   /* test stupid call */
   if( pP == pQ ) {
      ECCP521_DblPoint(pP, pR, pECC, pList);
      return;
   }

   /* prevent operation with point at Infinity */
   if( ECCP_IsPointAtInfinity(pP) ) {
      ECCP_CopyPoint(pQ, pR);
      return;
   }
   if( ECCP_IsPointAtInfinity(pQ) ) {
      ECCP_CopyPoint(pP, pR);
      return;
   }

   /*
   // addition
   */
   {
      IppsBigNumState* bnU0 = cpBigNumListGet(&pList);
      IppsBigNumState* bnS0 = cpBigNumListGet(&pList);
      IppsBigNumState* bnU1 = cpBigNumListGet(&pList);
      IppsBigNumState* bnS1 = cpBigNumListGet(&pList);
      IppsBigNumState* bnW  = cpBigNumListGet(&pList);
      IppsBigNumState* bnR  = cpBigNumListGet(&pList);
      IppsBigNumState *bnT  = bnU0;
      IppsBigNumState *bnM  = bnS0;

      /* U0 = P_X * Q_Z^2 */
      /* S0 = P_Y * Q_Z^3 */
      if( ECP_POINT_AFFINE(pQ) ) {
         PMA_set(bnU0, ECP_POINT_X(pP));
         PMA_set(bnS0, ECP_POINT_Y(pP));
      }
      else {
         PMA521_sqr(bnW, ECP_POINT_Z(pQ));
         PMA521_mul(bnU0,ECP_POINT_X(pP), bnW);
         PMA521_mul(bnW, ECP_POINT_Z(pQ), bnW);
         PMA521_mul(bnS0,ECP_POINT_Y(pP), bnW);
      }

      /* U1 = Q_X * P_Z^2 */
      /* S1 = Q_Y * P_Z^3 */
      if( ECP_POINT_AFFINE(pP) ) {
         PMA_set(bnU1, ECP_POINT_X(pQ));
         PMA_set(bnS1, ECP_POINT_Y(pQ));
      }
      else {
         PMA521_sqr(bnW, ECP_POINT_Z(pP));
         PMA521_mul(bnU1,ECP_POINT_X(pQ), bnW);
         PMA521_mul(bnW, ECP_POINT_Z(pP), bnW);
         PMA521_mul(bnS1,ECP_POINT_Y(pQ), bnW);
      }

      /* W = U0-U1 */
      /* R = S0-S1 */
      PMA521_sub(bnW, bnU0, bnU1);
      PMA521_sub(bnR, bnS0, bnS1);

      if( IsZero_BN(bnW) ) {
         if( IsZero_BN(bnR) ) {
            ECCP521_DblPoint(pP, pR, pECC, pList);
            return;
         }
         else {
            ECCP_SetPointToInfinity(pR);
            return;
         }
      }

      /* T = U0+U1 */
      /* M = S0+S1 */
      PMA521_add(bnT, bnU0, bnU1);
      PMA521_add(bnM, bnS0, bnS1);

      /* R_Z = P_Z * Q_Z * W */
      if( ECP_POINT_AFFINE(pQ) && ECP_POINT_AFFINE(pP) ) {
         PMA_set(ECP_POINT_Z(pR), bnW);
      }
      else {
         if( ECP_POINT_AFFINE(pQ) ) {
            PMA_set(bnU1, ECP_POINT_Z(pP));
         }
         else if( ECP_POINT_AFFINE(pP) ) {
            PMA_set(bnU1, ECP_POINT_Z(pQ));
         }
         else {
            PMA521_mul(bnU1, ECP_POINT_Z(pP), ECP_POINT_Z(pQ));
         }
         PMA521_mul(ECP_POINT_Z(pR), bnU1, bnW);
      }

      PMA521_sqr(bnU1, bnW);         /* U1 = W^2     */
      PMA521_mul(bnS1, bnT, bnU1);   /* S1 = T * W^2 */

      /* R_X = R^2 - T * W^2 */
      PMA521_sqr(ECP_POINT_X(pR), bnR);
      PMA521_sub(ECP_POINT_X(pR), ECP_POINT_X(pR), bnS1);

      /* V = T * W^2 - 2 * R_X  (S1) */
      PMA521_sub(bnS1, bnS1, ECP_POINT_X(pR));
      PMA521_sub(bnS1, bnS1, ECP_POINT_X(pR));

      /* R_Y = (V * R - M * W^3) /2 */
      PMA521_mul(ECP_POINT_Y(pR), bnS1, bnR);
      PMA521_mul(bnU1, bnU1, bnW);
      PMA521_mul(bnU1, bnU1, bnM);
      PMA521_sub(bnU1, ECP_POINT_Y(pR), bnU1);
      PMA521_div2(ECP_POINT_Y(pR), bnU1);

      ECP_POINT_AFFINE(pR) = 0;
   }
}

/*
// ECCP521_MulPoint
//
// Multiply point by scalar
*/
void ECCP521_MulPoint(const IppsECCPPointState* pP,
                   const IppsBigNumState* bnN,
                   IppsECCPPointState* pR,
                   const IppsECCPState* pECC,
                   BigNumNode* pList)
{
   /* test zero scalar or input point at Infinity */
   if( IsZero_BN(bnN) || ECCP_IsPointAtInfinity(pP) ) {
      ECCP_SetPointToInfinity(pR);
      return;
   }

   /*
   // scalar multiplication
   */
   else {
      IppsECCPPointState T;
      IppsECCPPointState U;
      IppsBigNumState* bnKH = cpBigNumListGet(&pList);
      Ipp32u* pK;
      Ipp32u* pH;
      Ipp32u carry;
      int lenKH;
      int bitH;

      /* init result */
      ECCP_CopyPoint(pP, pR);

      /* if scalar is negative */
      if( IppsBigNumNEG == BN_SIGN(bnN) ) {
         /* negative R */
         ECCP521_NegPoint(pR, pR, pECC);
      }

      /* copy K = N and compute H=3*K */
      lenKH = BN_SIZE(bnN)+1;
      pK = BN_NUMBER(bnKH);
      pH = BN_BUFFER(bnKH);
      Cpy_BNU(BN_NUMBER(bnN), pK, BN_SIZE(bnN));
      pK[lenKH-1] = 0;
      ippsAdd_BNU(pK, pK, pH, lenKH, &carry);
      ippsAdd_BNU(pK, pH, pH, lenKH, &carry);

      /* init temporary T = (X/Z^2, Y/Z^3, 1) */
      ECP_POINT_X(&T) = cpBigNumListGet(&pList);
      ECP_POINT_Y(&T) = cpBigNumListGet(&pList);
      ECP_POINT_Z(&T) = cpBigNumListGet(&pList);
      ECCP521_GetPointAffine(ECP_POINT_X(&T), ECP_POINT_Y(&T), pR, pECC, pList);
      ECCP521_SetPointAffine(ECP_POINT_X(&T), ECP_POINT_Y(&T), &T, pECC);

      /* temporary point U =-T */
      ECP_POINT_X(&U) = cpBigNumListGet(&pList);
      ECP_POINT_Y(&U) = cpBigNumListGet(&pList);
      ECP_POINT_Z(&U) = cpBigNumListGet(&pList);
      ECCP521_NegPoint(&T, &U, pECC);

      for(bitH=MSB_BNU(pH, lenKH)-1; bitH>0; bitH--) {
         int hBit = TST_BIT(pH, bitH);
         int kBit = TST_BIT(pK, bitH);
         ECCP521_DblPoint(pR, pR, pECC, pList);
         if( hBit && !kBit )
            ECCP521_AddPoint(pR, &T, pR, pECC, pList);
         if(!hBit &&  kBit )
            ECCP521_AddPoint(pR, &U, pR, pECC, pList);
      }
   }
}

/*
// ECCP521_ProdPoint
//
// Point product
*/
void ECCP521_ProdPoint(const IppsECCPPointState* pP,
                    const IppsBigNumState*    bnPscalar,
                    const IppsECCPPointState* pQ,
                    const IppsBigNumState*    bnQscalar,
                    IppsECCPPointState* pR,
                    const IppsECCPState* pECC,
                    BigNumNode* pList)
{
   /* test zero scalars */
   if( IsZero_BN(bnPscalar) ) {
      ECCP521_MulPoint(pQ, bnQscalar, pR, pECC, pList);
      return;
   }
   if( IsZero_BN(bnQscalar) ) {
      ECCP521_MulPoint(pP, bnPscalar, pR, pECC, pList);
      return;
   }

   /*
   // point product
   */
   else {
      int n;
      Ipp32u* pbnPscalar = BN_NUMBER(bnPscalar);
      int bnPscalarSize  = BN_SIZE(bnPscalar);
      Ipp32u* pbnQscalar = BN_NUMBER(bnQscalar);
      int bnQscalarSize  = BN_SIZE(bnQscalar);

      int size = bnPscalarSize>bnQscalarSize? bnPscalarSize : bnQscalarSize;

      IppsECCPPointState* pPointPQ[4] = {NULL, NULL, NULL, NULL};

      /* allocate temporary PQ point */
      IppsECCPPointState PQ;
      ECP_POINT_X(&PQ) = cpBigNumListGet(&pList);
      ECP_POINT_Y(&PQ) = cpBigNumListGet(&pList);
      ECP_POINT_Z(&PQ) = cpBigNumListGet(&pList);

      /* init temporary point array */
      if(IppsBigNumPOS == BN_SIGN(bnPscalar))
         pPointPQ[1] = (IppsECCPPointState*)pP;
      else {
         IppsECCPPointState P;
         ECP_POINT_X(&P) = cpBigNumListGet(&pList);
         ECP_POINT_Y(&P) = cpBigNumListGet(&pList);
         ECP_POINT_Z(&P) = cpBigNumListGet(&pList);
         ECCP521_NegPoint(pP, &P, pECC);
         pPointPQ[1] = &P;
      }
      if(IppsBigNumPOS == BN_SIGN(bnQscalar))
         pPointPQ[2] = (IppsECCPPointState*)pQ;
      else {
         IppsECCPPointState Q;
         ECP_POINT_X(&Q) = cpBigNumListGet(&pList);
         ECP_POINT_Y(&Q) = cpBigNumListGet(&pList);
         ECP_POINT_Z(&Q) = cpBigNumListGet(&pList);
         ECCP521_NegPoint(pQ, &Q, pECC);
         pPointPQ[2] = &Q;
      }

      ECCP521_AddPoint(pPointPQ[1], pPointPQ[2], &PQ, pECC, pList);
      ECCP521_GetPointAffine(ECP_POINT_X(pR), ECP_POINT_Y(pR), &PQ, pECC, pList);
      ECCP521_SetPointAffine(ECP_POINT_X(pR), ECP_POINT_Y(pR), &PQ, pECC);
      pPointPQ[3] = &PQ;

      /* pad scalars by zeros */
      ZEXPAND_BNU(pbnPscalar,bnPscalarSize, size);
      ZEXPAND_BNU(pbnQscalar,bnQscalarSize, size);

      /* init result */
      ECCP_SetPointToInfinity(pR);

      for(n=size; n>0; n--) {
         Ipp32u scalarPn = pbnPscalar[n-1];
         Ipp32u scalarQn = pbnQscalar[n-1];

         int nBit;
         for(nBit=31; nBit>=0; nBit--) {
            int
            PnQnBits = scalarPn&0x80000000? 1:0;
            PnQnBits+= scalarQn&0x80000000? 2:0;

            if( !ECCP_IsPointAtInfinity(pR) )
               ECCP521_DblPoint(pR, pR, pECC, pList);
            if( PnQnBits )
               ECCP521_AddPoint(pR, pPointPQ[PnQnBits], pR, pECC, pList);

            scalarPn <<= 1;
            scalarQn <<= 1;
         }
      }
   }
}

#endif /* !_IPP_TRS */

#endif /* _IPP_v50_ */
