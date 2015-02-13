/*//////////////////////////////////////////////////////////////////////////////
////                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014 Intel Corporation. All Rights Reserved.
//
*/

class SurfaceIndex;
extern "C" {
void DownSampleMB(SurfaceIndex, SurfaceIndex);
void DownSampleMB2t(SurfaceIndex, SurfaceIndex, SurfaceIndex);
void DownSampleMB4t(SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex);
void Ime(SurfaceIndex, SurfaceIndex, SurfaceIndex);
void Ime_4MV(SurfaceIndex, SurfaceIndex, SurfaceIndex);
void ImeWithPred(SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex);
void Ime3tiers4MV(SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex);
void Ime3tiers(SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex);
void MeP32(SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex, int);
void MeP32_4MV(SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex, int);
void MeP16_Intra(SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex);
void MeP16(SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex,
           SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex, int);
void MeP16_4MV(SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex,
           SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex, int);
void RefineMeP32x32(SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex);
void RefineMeP32x16(SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex);
void RefineMeP16x32(SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex);
void AnalyzeGradient(SurfaceIndex, SurfaceIndex, SurfaceIndex, unsigned int);
void AnalyzeGradient2(SurfaceIndex, SurfaceIndex, SurfaceIndex, unsigned int);
void AnalyzeGradient3(SurfaceIndex, SurfaceIndex, SurfaceIndex, unsigned int);
void AnalyzeGradient32x32Best(SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex, unsigned int);
void InterpolateFrame(SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex);
void Me1xAndInterpolateFrame(SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex,
                             SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex,
                             SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex,
                             SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex, SurfaceIndex,
                             SurfaceIndex);
};
