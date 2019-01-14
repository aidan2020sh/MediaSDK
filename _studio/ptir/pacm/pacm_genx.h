// Copyright (c) 2014-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

template<unsigned int PLANE_WIDTH, unsigned int PLANE_HEIGHT>
void cmk_media_deinterlace_nv12(SurfaceIndex scrFrameSurfaceId, SurfaceIndex dstTopFieldSurfaceId, SurfaceIndex dstBotFieldSurfaceId);

template<bool IS_TOP_FIELD, unsigned int PLANE_WIDTH, unsigned int PLANE_HEIGHT>
void cmk_media_deinterlace_single_field_nv12(SurfaceIndex scrFrameSurfaceId, SurfaceIndex dstFieldSurfaceId);

template<unsigned int PLANE_WIDTH, unsigned int PLANE_HEIGHT>
void cmk_sad_nv12(SurfaceIndex currFrameSurfaceId, SurfaceIndex prevFrameSurfaceId, SurfaceIndex sadSurfaceId);

template<unsigned int PLANE_WIDTH, unsigned int PLANE_HEIGHT, unsigned int TOP_FIELD>
void cmk_undo2frame_nv12(SurfaceIndex scrFrameSurfaceId, SurfaceIndex dstFrameSurfaceId);

template <unsigned int PLANE_WIDTH, unsigned int PLANE_HEIGHT, unsigned int TOP_FIELD>
void cmk_DeinterlaceBorder(SurfaceIndex inputSurface, unsigned int WIDTH, unsigned int HEIGHT);

template<unsigned int PLANE_WIDTH, unsigned int PLANE_HEIGHT>
void cmk_sad_rs_nv12(SurfaceIndex currFrameSurfaceId, SurfaceIndex prevFrameSurfaceId, SurfaceIndex sadSurfaceId, SurfaceIndex rsSurfaceId, int Height);

void cmk_FixEdgeDirectionalIYUV_Main_Bottom_Instance(SurfaceIndex surfIn, SurfaceIndex surfBadMC);
void cmk_FixEdgeDirectionalIYUV_Main_Top_Instance(SurfaceIndex surfIn, SurfaceIndex surfBadMC);

template<unsigned int PLANE_WIDTH, unsigned int PLANE_HEIGHT>
void cmk_rs_nv12(SurfaceIndex frameSurfaceId, SurfaceIndex rsSurfaceId, int Height);

template <unsigned int WIDTH, int BotBase, unsigned int THREADYSCALE>
void  cmk_FilterMask_Main(SurfaceIndex surfIn, SurfaceIndex surfBadMC, int Height);

template <unsigned int MAXWIDTH, int BotBase, unsigned int THREADYSCALE>
void cmk_FilterMask_Main_VarWidth(SurfaceIndex surfIn, SurfaceIndex surfBadMC, int Height, int Width);

template <unsigned int WIDTH, unsigned int THREADYSCALE>
void cmk_FilterMask_Main_2Fields(SurfaceIndex surfIn, SurfaceIndex surfIn2, SurfaceIndex surfBadMC, int Height);

template <unsigned int THREADYSCALE>
void cmk_FilterMask_Main_2Fields_VarWidth(SurfaceIndex surfIn, SurfaceIndex surfIn2, SurfaceIndex surfBadMC, int Height, int Width);

template<unsigned int PLANE_WIDTH, unsigned int PLANE_HEIGHT>
void cmk_sad_rs_nv12(SurfaceIndex currFrameSurfaceId, SurfaceIndex prevFrameSurfaceId, SurfaceIndex sadSurfaceId, SurfaceIndex rsSurfaceId, int Height);
