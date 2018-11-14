// Copyright (c) 2012-2018 Intel Corporation
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

#include <cm/cm.h>
#include <cm/cmtl.h>

_GENX_ 
void inline Sample16x4(
	SamplerIndex				sampler,
	SurfaceIndex				input,
	float						x_top_left,
	float						y_top_left,
	uint						width,
	uint						height,
	matrix_ref<uchar, 4, 16>	Y,
	matrix_ref<uchar, 2, 16>	UV)
{
	cm_vector(offset, float, 16, 0, 1);
	matrix<float, 4, 16> line;

	vector<float, 16> x = x_top_left + offset / width;
	for (int i = 0; i < 4; i++) {
		vector<float, 16> y= y_top_left + 1.0f * i / height;

		sample16(line, CM_BGR_ENABLE, input, sampler, x, y);
		line *= 255;
		
		Y.select<1, 1, 16, 1>(i, 0) = line.row(1);
		if (i % 2 == 0) {
			UV.select<1, 1, 8, 2>(i >> 1, 0) = line.select<1, 1, 8, 2>(2, 0);
			UV.select<1, 1, 8, 2>(i >> 1, 1) = line.select<1, 1, 8, 2>(0, 0);
		}
	}
}


extern "C" _GENX_MAIN_  
	void camera_pipe (
	SurfaceIndex	sampler_params,
	SurfaceIndex	output,
	SurfaceIndex	input,
	SamplerIndex	sampler,
	uint			width,
	uint			height)
{
	uint h_pos = get_thread_origin_x();
	uint v_pos = get_thread_origin_y();

	// Process 4 blocks of 16x4 size
	for (int i = 0; i < 4; i++) {
		uint x_pos = h_pos * 16;
		uint y_pos = v_pos * 16 + i * 4;

		// Reading sampler params for red channel
		vector<float,6> sampler_args;
		read(sampler_params, h_pos * 24, v_pos * 4 + i, sampler_args);

		float u0 = sampler_args[0];
		float v0 = sampler_args[3];

		printf("%d\t%d\t%f\t%f\n",
			v_pos * 4 + i, h_pos, u0, v0);

		matrix<uchar, 4, 16> Y;
		matrix<uchar, 2, 16> UV;

		// Process 1 block of 16x4 size
		Sample16x4(sampler, input, u0, v0, width, height, Y, UV);

		write_plane(output, GENX_SURFACE_Y_PLANE,  x_pos, y_pos,     Y);
		write_plane(output, GENX_SURFACE_UV_PLANE, x_pos, y_pos / 2, UV);
	}
}
