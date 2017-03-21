//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2011-2017 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"


#if defined (UMC_ENABLE_VP8_VIDEO_DECODER)

#include "vp8_dec_defs.h"

using namespace UMC;

namespace UMC
{

const Ipp8u vp8_kf_mb_mode_y_probs[VP8_NUM_MB_MODES_Y - 1]   = {145, 156, 163, 128};
const Ipp8u vp8_kf_mb_mode_uv_probs[VP8_NUM_MB_MODES_UV - 1] = {142, 114, 183};


const Ipp8u vp8_kf_block_mode_probs[VP8_NUM_INTRA_BLOCK_MODES][VP8_NUM_INTRA_BLOCK_MODES][VP8_NUM_INTRA_BLOCK_MODES-1] = 
{
  {
    { 231, 120, 48, 89, 115, 113, 120, 152, 112},
    { 152, 179, 64, 126, 170, 118, 46, 70, 95},
    { 175, 69, 143, 80, 85, 82, 72, 155, 103},
    { 56, 58, 10, 171, 218, 189, 17, 13, 152},
    { 144, 71, 10, 38, 171, 213, 144, 34, 26},
    { 114, 26, 17, 163, 44, 195, 21, 10, 173},
    { 121, 24, 80, 195, 26, 62, 44, 64, 85},
    { 170, 46, 55, 19, 136, 160, 33, 206, 71},
    { 63, 20, 8, 114, 114, 208, 12, 9, 226},
    { 81, 40, 11, 96, 182, 84, 29, 16, 36}
  },
  {
    { 134, 183, 89, 137, 98, 101, 106, 165, 148},
    { 72, 187, 100, 130, 157, 111, 32, 75, 80},
    { 66, 102, 167, 99, 74, 62, 40, 234, 128},
    { 41, 53, 9, 178, 241, 141, 26, 8, 107},
    { 104, 79, 12, 27, 217, 255, 87, 17, 7},
    { 74, 43, 26, 146, 73, 166, 49, 23, 157},
    { 65, 38, 105, 160, 51, 52, 31, 115, 128},
    { 87, 68, 71, 44, 114, 51, 15, 186, 23},
    { 47, 41, 14, 110, 182, 183, 21, 17, 194},
    { 66, 45, 25, 102, 197, 189, 23, 18, 22}
  },
  {
    { 88, 88, 147, 150, 42, 46, 45, 196, 205},
    { 43, 97, 183, 117, 85, 38, 35, 179, 61},
    { 39, 53, 200, 87, 26, 21, 43, 232, 171},
    { 56, 34, 51, 104, 114, 102, 29, 93, 77},
    { 107, 54, 32, 26, 51, 1, 81, 43, 31},
    { 39, 28, 85, 171, 58, 165, 90, 98, 64},
    { 34, 22, 116, 206, 23, 34, 43, 166, 73},
    { 68, 25, 106, 22, 64, 171, 36, 225, 114},
    { 34, 19, 21, 102, 132, 188, 16, 76, 124},
    { 62, 18, 78, 95, 85, 57, 50, 48, 51}
  },
  {
    { 193, 101, 35, 159, 215, 111, 89, 46, 111},
    { 60, 148, 31, 172, 219, 228, 21, 18, 111},
    { 112, 113, 77, 85, 179, 255, 38, 120, 114},
    { 40, 42, 1, 196, 245, 209, 10, 25, 109},
    { 100, 80, 8, 43, 154, 1, 51, 26, 71},
    { 88, 43, 29, 140, 166, 213, 37, 43, 154},
    { 61, 63, 30, 155, 67, 45, 68, 1, 209},
    { 142, 78, 78, 16, 255, 128, 34, 197, 171},
    { 41, 40, 5, 102, 211, 183, 4, 1, 221},
    { 51, 50, 17, 168, 209, 192, 23, 25, 82}
  },
  {
    { 125, 98, 42, 88, 104, 85, 117, 175, 82},
    { 95, 84, 53, 89, 128, 100, 113, 101, 45},
    { 75, 79, 123, 47, 51, 128, 81, 171, 1},
    { 57, 17, 5, 71, 102, 57, 53, 41, 49},
    { 115, 21, 2, 10, 102, 255, 166, 23, 6},
    { 38, 33, 13, 121, 57, 73, 26, 1, 85},
    { 41, 10, 67, 138, 77, 110, 90, 47, 114},
    { 101, 29, 16, 10, 85, 128, 101, 196, 26},
    { 57, 18, 10, 102, 102, 213, 34, 20, 43},
    { 117, 20, 15, 36, 163, 128, 68, 1, 26}
  },
  {
    { 138, 31, 36, 171, 27, 166, 38, 44, 229},
    { 67, 87, 58, 169, 82, 115, 26, 59, 179},
    { 63, 59, 90, 180, 59, 166, 93, 73, 154},
    { 40, 40, 21, 116, 143, 209, 34, 39, 175},
    { 57, 46, 22, 24, 128, 1, 54, 17, 37},
    { 47, 15, 16, 183, 34, 223, 49, 45, 183},
    { 46, 17, 33, 183, 6, 98, 15, 32, 183},
    { 65, 32, 73, 115, 28, 128, 23, 128, 205},
    { 40, 3, 9, 115, 51, 192, 18, 6, 223},
    { 87, 37, 9, 115, 59, 77, 64, 21, 47}
  },
  {
    { 104, 55, 44, 218, 9, 54, 53, 130, 226},
    { 64, 90, 70, 205, 40, 41, 23, 26, 57},
    { 54, 57, 112, 184, 5, 41, 38, 166, 213},
    { 30, 34, 26, 133, 152, 116, 10, 32, 134},
    { 75, 32, 12, 51, 192, 255, 160, 43, 51},
    { 39, 19, 53, 221, 26, 114, 32, 73, 255},
    { 31, 9, 65, 234, 2, 15, 1, 118, 73},
    { 88, 31, 35, 67, 102, 85, 55, 186, 85},
    { 56, 21, 23, 111, 59, 205, 45, 37, 192},
    { 55, 38, 70, 124, 73, 102, 1, 34, 98}
  },
  {
    { 102, 61, 71, 37, 34, 53, 31, 243, 192},
    { 69, 60, 71, 38, 73, 119, 28, 222, 37},
    { 68, 45, 128, 34, 1, 47, 11, 245, 171},
    { 62, 17, 19, 70, 146, 85, 55, 62, 70},
    { 75, 15, 9, 9, 64, 255, 184, 119, 16},
    { 37, 43, 37, 154, 100, 163, 85, 160, 1},
    { 63, 9, 92, 136, 28, 64, 32, 201, 85},
    { 86, 6, 28, 5, 64, 255, 25, 248, 1},
    { 56, 8, 17, 132, 137, 255, 55, 116, 128},
    { 58, 15, 20, 82, 135, 57, 26, 121, 40}
  },
  {
    { 164, 50, 31, 137, 154, 133, 25, 35, 218},
    { 51, 103, 44, 131, 131, 123, 31, 6, 158},
    { 86, 40, 64, 135, 148, 224, 45, 183, 128},
    { 22, 26, 17, 131, 240, 154, 14, 1, 209},
    { 83, 12, 13, 54, 192, 255, 68, 47, 28},
    { 45, 16, 21, 91, 64, 222, 7, 1, 197},
    { 56, 21, 39, 155, 60, 138, 23, 102, 213},
    { 85, 26, 85, 85, 128, 128, 32, 146, 171},
    { 18, 11, 7, 63, 144, 171, 4, 4, 246},
    { 35, 27, 10, 146, 174, 171, 12, 26, 128}
  },
  {
    { 190, 80, 35, 99, 180, 80, 126, 54, 45},
    { 85, 126, 47, 87, 176, 51, 41, 20, 32},
    { 101, 75, 128, 139, 118, 146, 116, 128, 85},
    { 56, 41, 15, 176, 236, 85, 37, 9, 62},
    { 146, 36, 19, 30, 171, 255, 97, 27, 20},
    { 71, 30, 17, 119, 118, 255, 17, 18, 138},
    { 101, 38, 60, 138, 55, 70, 43, 26, 142},
    { 138, 45, 61, 62, 219, 1, 81, 188, 64},
    { 32, 41, 20, 117, 151, 142, 20, 21, 163},
    { 112, 19, 12, 61, 195, 128, 48, 4, 24}
  }
};

const Ipp8u vp8_mb_mode_y_probs[VP8_NUM_MB_MODES_Y - 1] = {112, 86, 140, 37};

const Ipp8u vp8_mb_mode_uv_probs[VP8_NUM_MB_MODES_UV - 1] = {162, 101, 204};

const Ipp8u vp8_block_mode_probs [VP8_NUM_INTRA_BLOCK_MODES - 1] = {
  120, 90, 79, 133, 87, 85, 80, 111, 151
};


const Ipp8u vp8_default_coeff_probs[VP8_NUM_COEFF_PLANES][VP8_NUM_COEFF_BANDS][VP8_NUM_LOCAL_COMPLEXITIES][VP8_NUM_COEFF_NODES] =
{
  {
    {
      { 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128},
      { 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128},
      { 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128}
    },
    {
      { 253, 136, 254, 255, 228, 219, 128, 128, 128, 128, 128},
      { 189, 129, 242, 255, 227, 213, 255, 219, 128, 128, 128},
      { 106, 126, 227, 252, 214, 209, 255, 255, 128, 128, 128}
    },
    {
      { 1, 98, 248, 255, 236, 226, 255, 255, 128, 128, 128},
      { 181, 133, 238, 254, 221, 234, 255, 154, 128, 128, 128},
      { 78, 134, 202, 247, 198, 180, 255, 219, 128, 128, 128}
    },
    {
      { 1, 185, 249, 255, 243, 255, 128, 128, 128, 128, 128},
      { 184, 150, 247, 255, 236, 224, 128, 128, 128, 128, 128},
      { 77, 110, 216, 255, 236, 230, 128, 128, 128, 128, 128}
    },
    {
      { 1, 101, 251, 255, 241, 255, 128, 128, 128, 128, 128},
      { 170, 139, 241, 252, 236, 209, 255, 255, 128, 128, 128},
      { 37, 116, 196, 243, 228, 255, 255, 255, 128, 128, 128}
    },
    {
      { 1, 204, 254, 255, 245, 255, 128, 128, 128, 128, 128},
      { 207, 160, 250, 255, 238, 128, 128, 128, 128, 128, 128},
      { 102, 103, 231, 255, 211, 171, 128, 128, 128, 128, 128}
    },
    {
      { 1, 152, 252, 255, 240, 255, 128, 128, 128, 128, 128},
      { 177, 135, 243, 255, 234, 225, 128, 128, 128, 128, 128},
      { 80, 129, 211, 255, 194, 224, 128, 128, 128, 128, 128}
    },
    {
      { 1, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128},
      { 246, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128},
      { 255, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128}
    }
  },
  {
    {
      { 198, 35, 237, 223, 193, 187, 162, 160, 145, 155, 62},
      { 131, 45, 198, 221, 172, 176, 220, 157, 252, 221, 1},
      { 68, 47, 146, 208, 149, 167, 221, 162, 255, 223, 128}
    },
    {
      { 1, 149, 241, 255, 221, 224, 255, 255, 128, 128, 128},
      { 184, 141, 234, 253, 222, 220, 255, 199, 128, 128, 128},
      { 81, 99, 181, 242, 176, 190, 249, 202, 255, 255, 128}
    },
    {
      { 1, 129, 232, 253, 214, 197, 242, 196, 255, 255, 128},
      { 99, 121, 210, 250, 201, 198, 255, 202, 128, 128, 128},
      { 23, 91, 163, 242, 170, 187, 247, 210, 255, 255, 128}
    },
    {
      { 1, 200, 246, 255, 234, 255, 128, 128, 128, 128, 128},
      { 109, 178, 241, 255, 231, 245, 255, 255, 128, 128, 128},
      { 44, 130, 201, 253, 205, 192, 255, 255, 128, 128, 128}
    },
    {
      { 1, 132, 239, 251, 219, 209, 255, 165, 128, 128, 128},
      { 94, 136, 225, 251, 218, 190, 255, 255, 128, 128, 128},
      { 22, 100, 174, 245, 186, 161, 255, 199, 128, 128, 128}
    },
    {
      { 1, 182, 249, 255, 232, 235, 128, 128, 128, 128, 128},
      { 124, 143, 241, 255, 227, 234, 128, 128, 128, 128, 128},
      { 35, 77, 181, 251, 193, 211, 255, 205, 128, 128, 128}
    },
    {
      { 1, 157, 247, 255, 236, 231, 255, 255, 128, 128, 128},
      { 121, 141, 235, 255, 225, 227, 255, 255, 128, 128, 128},
      { 45, 99, 188, 251, 195, 217, 255, 224, 128, 128, 128}
    },
    {
      { 1, 1, 251, 255, 213, 255, 128, 128, 128, 128, 128},
      { 203, 1, 248, 255, 255, 128, 128, 128, 128, 128, 128},
      { 137, 1, 177, 255, 224, 255, 128, 128, 128, 128, 128}
    }
  },
  {
    {
      { 253, 9, 248, 251, 207, 208, 255, 192, 128, 128, 128},
      { 175, 13, 224, 243, 193, 185, 249, 198, 255, 255, 128},
      { 73, 17, 171, 221, 161, 179, 236, 167, 255, 234, 128}
    },
    {
      { 1, 95, 247, 253, 212, 183, 255, 255, 128, 128, 128},
      { 239, 90, 244, 250, 211, 209, 255, 255, 128, 128, 128},
      { 155, 77, 195, 248, 188, 195, 255, 255, 128, 128, 128}
    },
    {
      { 1, 24, 239, 251, 218, 219, 255, 205, 128, 128, 128},
      { 201, 51, 219, 255, 196, 186, 128, 128, 128, 128, 128},
      { 69, 46, 190, 239, 201, 218, 255, 228, 128, 128, 128}
    },
    {
      { 1, 191, 251, 255, 255, 128, 128, 128, 128, 128, 128},
      { 223, 165, 249, 255, 213, 255, 128, 128, 128, 128, 128},
      { 141, 124, 248, 255, 255, 128, 128, 128, 128, 128, 128}
    },
    {
      { 1, 16, 248, 255, 255, 128, 128, 128, 128, 128, 128},
      { 190, 36, 230, 255, 236, 255, 128, 128, 128, 128, 128},
      { 149, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128}
    },
    {
      { 1, 226, 255, 128, 128, 128, 128, 128, 128, 128, 128},
      { 247, 192, 255, 128, 128, 128, 128, 128, 128, 128, 128},
      { 240, 128, 255, 128, 128, 128, 128, 128, 128, 128, 128}
    },
    {
      { 1, 134, 252, 255, 255, 128, 128, 128, 128, 128, 128},
      { 213, 62, 250, 255, 255, 128, 128, 128, 128, 128, 128},
      { 55, 93, 255, 128, 128, 128, 128, 128, 128, 128, 128}
    },
    {
      { 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128},
      { 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128},
      { 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128}
    }
  },
  {
    {
      { 202, 24, 213, 235, 186, 191, 220, 160, 240, 175, 255},
      { 126, 38, 182, 232, 169, 184, 228, 174, 255, 187, 128},
      { 61, 46, 138, 219, 151, 178, 240, 170, 255, 216, 128}
    },
    {
      { 1, 112, 230, 250, 199, 191, 247, 159, 255, 255, 128},
      { 166, 109, 228, 252, 211, 215, 255, 174, 128, 128, 128},
      { 39, 77, 162, 232, 172, 180, 245, 178, 255, 255, 128}
    },
    {
      { 1, 52, 220, 246, 198, 199, 249, 220, 255, 255, 128},
      { 124, 74, 191, 243, 183, 193, 250, 221, 255, 255, 128},
      { 24, 71, 130, 219, 154, 170, 243, 182, 255, 255, 128}
    },
    {
      { 1, 182, 225, 249, 219, 240, 255, 224, 128, 128, 128},
      { 149, 150, 226, 252, 216, 205, 255, 171, 128, 128, 128},
      { 28, 108, 170, 242, 183, 194, 254, 223, 255, 255, 128}
    },
    {
      { 1, 81, 230, 252, 204, 203, 255, 192, 128, 128, 128},
      { 123, 102, 209, 247, 188, 196, 255, 233, 128, 128, 128},
      { 20, 95, 153, 243, 164, 173, 255, 203, 128, 128, 128}
    },
    {
      { 1, 222, 248, 255, 216, 213, 128, 128, 128, 128, 128},
      { 168, 175, 246, 252, 235, 205, 255, 255, 128, 128, 128},
      { 47, 116, 215, 255, 211, 212, 255, 255, 128, 128, 128}
    },
    {
      { 1, 121, 236, 253, 212, 214, 255, 255, 128, 128, 128},
      { 141, 84, 213, 252, 201, 202, 255, 219, 128, 128, 128},
      { 42, 80, 160, 240, 162, 185, 255, 205, 128, 128, 128}
    },
    {
      { 1, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128},
      { 244, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128},
      { 238, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128}
    }
  }
};

const Ipp8u vp8_coeff_update_probs[VP8_NUM_COEFF_PLANES][VP8_NUM_COEFF_BANDS][VP8_NUM_LOCAL_COMPLEXITIES][VP8_NUM_COEFF_NODES] =
{
  {
    {
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255}
    },
    {
      { 176, 246, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      { 223, 241, 252, 255, 255, 255, 255, 255, 255, 255, 255},
      { 249, 253, 253, 255, 255, 255, 255, 255, 255, 255, 255}
    },
    {
      { 255, 244, 252, 255, 255, 255, 255, 255, 255, 255, 255},
      { 234, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255},
      { 253, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255}
    },
    {
      { 255, 246, 254, 255, 255, 255, 255, 255, 255, 255, 255},
      { 239, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255},
      { 254, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255}
    },
    {
      { 255, 248, 254, 255, 255, 255, 255, 255, 255, 255, 255},
      { 251, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255},
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255}
    },
    {
      { 255, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255},
      { 251, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255},
      { 254, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255}
    },
    {
      { 255, 254, 253, 255, 254, 255, 255, 255, 255, 255, 255},
      { 250, 255, 254, 255, 254, 255, 255, 255, 255, 255, 255},
      { 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255}
    },
    {
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255}
    }
  },
  {
    {
      { 217, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      { 225, 252, 241, 253, 255, 255, 254, 255, 255, 255, 255},
      { 234, 250, 241, 250, 253, 255, 253, 254, 255, 255, 255}
    },
    {
      { 255, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      { 223, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255},
      { 238, 253, 254, 254, 255, 255, 255, 255, 255, 255, 255}
    },
    {
      { 255, 248, 254, 255, 255, 255, 255, 255, 255, 255, 255},
      { 249, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255}
    },
    {
      { 255, 253, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      { 247, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255}
    },
    {
      { 255, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255},
      { 252, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255}
    },
    {
      { 255, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255},
      { 253, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255}
    },
    {
      { 255, 254, 253, 255, 255, 255, 255, 255, 255, 255, 255},
      { 250, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      { 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255}
    },
    {
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255}
    }
  },
  {
    {
      { 186, 251, 250, 255, 255, 255, 255, 255, 255, 255, 255},
      { 234, 251, 244, 254, 255, 255, 255, 255, 255, 255, 255},
      { 251, 251, 243, 253, 254, 255, 254, 255, 255, 255, 255}
    },
    {
      { 255, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255},
      { 236, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255},
      { 251, 253, 253, 254, 254, 255, 255, 255, 255, 255, 255}
    },
    {
      { 255, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255},
      { 254, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255},
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255}
    },
    {
      { 255, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      { 254, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      { 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255}
    },
    {
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      { 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255}
    },
    {
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255}
    },
    {
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255}
    },
    {
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255}
    }
  },
  {
    {
      { 248, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      { 250, 254, 252, 254, 255, 255, 255, 255, 255, 255, 255},
      { 248, 254, 249, 253, 255, 255, 255, 255, 255, 255, 255}
    },
    {
      { 255, 253, 253, 255, 255, 255, 255, 255, 255, 255, 255},
      { 246, 253, 253, 255, 255, 255, 255, 255, 255, 255, 255},
      { 252, 254, 251, 254, 254, 255, 255, 255, 255, 255, 255}
    },
    {
      { 255, 254, 252, 255, 255, 255, 255, 255, 255, 255, 255},
      { 248, 254, 253, 255, 255, 255, 255, 255, 255, 255, 255},
      { 253, 255, 254, 254, 255, 255, 255, 255, 255, 255, 255}
    },
    {
      { 255, 251, 254, 255, 255, 255, 255, 255, 255, 255, 255},
      { 245, 251, 254, 255, 255, 255, 255, 255, 255, 255, 255},
      { 253, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255}
    },
    {
      { 255, 251, 253, 255, 255, 255, 255, 255, 255, 255, 255},
      { 252, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255},
      { 255, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255}
    },
    {
      { 255, 252, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      { 249, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255},
      { 255, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255}
    },
    {
      { 255, 255, 253, 255, 255, 255, 255, 255, 255, 255, 255},
      { 250, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255}
    },
    {
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      { 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255},
      { 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255}
    }
  }
};


const Ipp8s vp8_mb_mode_y_tree[2*(VP8_NUM_MB_MODES_Y - 1)] =
{
  -VP8_MB_DC_PRED, 2, 4, 6,
  -VP8_MB_V_PRED, -VP8_MB_H_PRED, -VP8_MB_TM_PRED, -VP8_MB_B_PRED
};


const Ipp8s vp8_kf_mb_mode_y_tree[2*(VP8_NUM_MB_MODES_Y - 1)] =
{
  -VP8_MB_B_PRED, 2, 4, 6,
  -VP8_MB_DC_PRED, -VP8_MB_V_PRED, -VP8_MB_H_PRED, -VP8_MB_TM_PRED
};


const Ipp8s vp8_mb_mode_uv_tree[2*(VP8_NUM_MB_MODES_UV - 1)] =
{
  -VP8_MB_DC_PRED, 2, -VP8_MB_V_PRED, 4, -VP8_MB_H_PRED, -VP8_MB_TM_PRED
};


const Ipp8s vp8_block_mode_tree[2*(VP8_NUM_INTRA_BLOCK_MODES - 1)] =
{
  -VP8_B_DC_PRED, 2,                 /* B_DC_PRED = "0" */
  -VP8_B_TM_PRED, 4,                 /* B_TM_PRED = "10" */
  -VP8_B_VE_PRED, 6,                 /* B_VE_PRED = "110" */
   8, 12,
  -VP8_B_HE_PRED, 10,                /* B_HE_PRED = "1110" */
  -VP8_B_RD_PRED, -VP8_B_VR_PRED,    /* B_RD_PRED = "111100", B_VR_PRED = "111101" */
  -VP8_B_LD_PRED, 14,                /* B_LD_PRED = "111110" */
  -VP8_B_VL_PRED, 16,                /* B_VL_PRED = "1111110" */
  -VP8_B_HD_PRED, -VP8_B_HU_PRED     /* HD = "11111110", HU = "11111111" */
};


const Ipp32u vp8_mbmode_2_blockmode_u32[VP8_NUM_MB_MODES_Y] =
{
  (VP8_B_DC_PRED << 24) | (VP8_B_DC_PRED << 16)  | (VP8_B_DC_PRED << 8) | VP8_B_DC_PRED,
  (VP8_B_VE_PRED << 24) | (VP8_B_VE_PRED << 16)  | (VP8_B_VE_PRED << 8) | VP8_B_VE_PRED,
  (VP8_B_HE_PRED << 24) | (VP8_B_HE_PRED << 16)  | (VP8_B_HE_PRED << 8) | VP8_B_HE_PRED,
  (VP8_B_TM_PRED << 24) | (VP8_B_TM_PRED << 16)  | (VP8_B_TM_PRED << 8) | VP8_B_TM_PRED,
  (VP8_B_DC_PRED << 24) | (VP8_B_DC_PRED << 16)  | (VP8_B_DC_PRED << 8) | VP8_B_DC_PRED
};


const Ipp8u vp8_mv_update_probs[2][VP8_NUM_MV_PROBS] =
{
  {
    237,
    246,
    253, 253, 254, 254, 254, 254, 254,
    254, 254, 254, 254, 254, 250, 250, 252, 254, 254
  },
  {
    231,
    243,
    245, 253, 254, 254, 254, 254, 254,
    254, 254, 254, 254, 254, 251, 251, 254, 254, 254
  }
};


const Ipp8u vp8_default_mv_contexts[2][VP8_NUM_MV_PROBS] =
{
  { // y
    162, // is short
    128, // sign
    225, 146, 172, 147, 214, 39, 156, // short tree
    128, 129, 132, 75, 145, 178, 206, 239, 254, 254 // long bits
  },
  { // x
    164, 
    128,
    204, 170, 119, 235, 140, 230, 228,
    128, 130, 130, 74, 148, 180, 203, 236, 254, 254 // long bits
  }
};


//????
const Ipp8u vp8_ClampTbl[768] =
{
     0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00
    ,0x00 ,0x01 ,0x02 ,0x03 ,0x04 ,0x05 ,0x06 ,0x07
    ,0x08 ,0x09 ,0x0a ,0x0b ,0x0c ,0x0d ,0x0e ,0x0f
    ,0x10 ,0x11 ,0x12 ,0x13 ,0x14 ,0x15 ,0x16 ,0x17
    ,0x18 ,0x19 ,0x1a ,0x1b ,0x1c ,0x1d ,0x1e ,0x1f
    ,0x20 ,0x21 ,0x22 ,0x23 ,0x24 ,0x25 ,0x26 ,0x27
    ,0x28 ,0x29 ,0x2a ,0x2b ,0x2c ,0x2d ,0x2e ,0x2f
    ,0x30 ,0x31 ,0x32 ,0x33 ,0x34 ,0x35 ,0x36 ,0x37
    ,0x38 ,0x39 ,0x3a ,0x3b ,0x3c ,0x3d ,0x3e ,0x3f
    ,0x40 ,0x41 ,0x42 ,0x43 ,0x44 ,0x45 ,0x46 ,0x47
    ,0x48 ,0x49 ,0x4a ,0x4b ,0x4c ,0x4d ,0x4e ,0x4f
    ,0x50 ,0x51 ,0x52 ,0x53 ,0x54 ,0x55 ,0x56 ,0x57
    ,0x58 ,0x59 ,0x5a ,0x5b ,0x5c ,0x5d ,0x5e ,0x5f
    ,0x60 ,0x61 ,0x62 ,0x63 ,0x64 ,0x65 ,0x66 ,0x67
    ,0x68 ,0x69 ,0x6a ,0x6b ,0x6c ,0x6d ,0x6e ,0x6f
    ,0x70 ,0x71 ,0x72 ,0x73 ,0x74 ,0x75 ,0x76 ,0x77
    ,0x78 ,0x79 ,0x7a ,0x7b ,0x7c ,0x7d ,0x7e ,0x7f
    ,0x80 ,0x81 ,0x82 ,0x83 ,0x84 ,0x85 ,0x86 ,0x87
    ,0x88 ,0x89 ,0x8a ,0x8b ,0x8c ,0x8d ,0x8e ,0x8f
    ,0x90 ,0x91 ,0x92 ,0x93 ,0x94 ,0x95 ,0x96 ,0x97
    ,0x98 ,0x99 ,0x9a ,0x9b ,0x9c ,0x9d ,0x9e ,0x9f
    ,0xa0 ,0xa1 ,0xa2 ,0xa3 ,0xa4 ,0xa5 ,0xa6 ,0xa7
    ,0xa8 ,0xa9 ,0xaa ,0xab ,0xac ,0xad ,0xae ,0xaf
    ,0xb0 ,0xb1 ,0xb2 ,0xb3 ,0xb4 ,0xb5 ,0xb6 ,0xb7
    ,0xb8 ,0xb9 ,0xba ,0xbb ,0xbc ,0xbd ,0xbe ,0xbf
    ,0xc0 ,0xc1 ,0xc2 ,0xc3 ,0xc4 ,0xc5 ,0xc6 ,0xc7
    ,0xc8 ,0xc9 ,0xca ,0xcb ,0xcc ,0xcd ,0xce ,0xcf
    ,0xd0 ,0xd1 ,0xd2 ,0xd3 ,0xd4 ,0xd5 ,0xd6 ,0xd7
    ,0xd8 ,0xd9 ,0xda ,0xdb ,0xdc ,0xdd ,0xde ,0xdf
    ,0xe0 ,0xe1 ,0xe2 ,0xe3 ,0xe4 ,0xe5 ,0xe6 ,0xe7
    ,0xe8 ,0xe9 ,0xea ,0xeb ,0xec ,0xed ,0xee ,0xef
    ,0xf0 ,0xf1 ,0xf2 ,0xf3 ,0xf4 ,0xf5 ,0xf6 ,0xf7
    ,0xf8 ,0xf9 ,0xfa ,0xfb ,0xfc ,0xfd ,0xfe ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
};



} // namespace UMC

#endif // UMC_ENABLE_VP8_VIDEO_DECODER
