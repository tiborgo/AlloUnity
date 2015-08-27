/*
 * Copyright 1993-2015 NVIDIA Corporation.  All rights reserved.
 *
 * Please refer to the NVIDIA end user license agreement (EULA) associated
 * with this source code for terms and conditions that govern your use of
 * this software. Any use, reproduction, disclosure, or distribution of
 * this software and related documentation outside the terms of the EULA
 * is strictly prohibited.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <helper_cuda.h>

#define PI 3.1415926536f

texture<uchar4, cudaTextureType2D, cudaReadModeElementType> texRef;
//texture<float4, 2, cudaReadModeElementType> texRef;

/*
 * Paint a 2D texture with a moving red/green hatch pattern on a
 * strobing blue background.  Note that this kernel reads to and
 * writes from the texture, hence why this texture was not mapped
 * as WriteDiscard.
 */
__global__ void cuda_kernel_texture_2d(unsigned char *surface, int width, int height, size_t pitch, float t)
{
    int x = blockIdx.x*blockDim.x + threadIdx.x;
    int y = blockIdx.y*blockDim.y + threadIdx.y;
    uint8_t *pixel;

    // in the case where, due to quantization into grids, we have
    // more threads than pixels, skip the threads which don't
    // correspond to valid pixels
    if (x >= width || y >= height) return;

	uchar4 src = tex2D(texRef, x, y);

    // get a pointer to the pixel at (x,y)
	pixel = (uint8_t*)(surface + y*pitch) + 4 * x;

    // populate it
    float value_x = 0.5f + 0.5f*cos(t + 10.0f*((2.0f*x)/width  - 1.0f));
    float value_y = 0.5f + 0.5f*cos(t + 10.0f*((2.0f*y)/height - 1.0f));
	if (src.y == 0 && src.x == 0 && src.z == 0 && src.w == 0)
	{
		pixel[0] = 0;// texRef 255 * (0.5*pixel[0] + 0.5*pow(value_x, 3.0f)); // red
		pixel[1] = 0;// 255 * (0.5*pixel[1] + 0.5*pow(value_y, 3.0f)); // green
		pixel[2] = 0;// 255 * (0.5f + 0.5f*cos(t)); // blue
	}
	else
	{
		pixel[0] = 255;// texRef 255 * (0.5*pixel[0] + 0.5*pow(value_x, 3.0f)); // red
		pixel[1] = 0;// 255 * (0.5*pixel[1] + 0.5*pow(value_y, 3.0f)); // green
		pixel[2] = 0;// 255 * (0.5f + 0.5f*cos(t)); // blue
	}
	
	pixel[0] = src.x;
	pixel[1] = src.y;
	pixel[2] = src.z;

	pixel[3] = 0; // alpha
}

extern "C"
void* cuda_texture_2d(cudaGraphicsResource* cudaResource, int width, int height, float t)
{
    cudaError_t error = cudaSuccess;

	//texture<uint8_t, cudaTextureType2D, cudaReadModeElementType> texRef;

	cudaArray* cuArray;

	void* cudaLinearMemory;
	size_t pitch;

	cudaMallocPitch(&cudaLinearMemory, &pitch, width * sizeof(uint8_t) * 4, height);
	getLastCudaError("cudaMallocPitch (g_texture_2d) failed");
	cudaMemset(cudaLinearMemory, 1, pitch * height);

	error = cudaGraphicsSubResourceGetMappedArray(&cuArray, cudaResource, 0, 0);
	getLastCudaError("cudaGraphicsSubResourceGetMappedArray (cuda_texture_2d) failed");

	error = cudaBindTextureToArray(texRef, cuArray);
	getLastCudaError("cudaGraphicsSubResourceGetMappedArray (cuda_texture_2d) failed");

    dim3 Db = dim3(16, 16);   // block dimensions are fixed to be 256 threads
    dim3 Dg = dim3((width+Db.x-1)/Db.x, (height+Db.y-1)/Db.y);

    cuda_kernel_texture_2d<<<Dg,Db>>>((unsigned char*)cudaLinearMemory, width, height, pitch, t);

    error = cudaGetLastError();

    if (error != cudaSuccess)
    {
        printf("cuda_kernel_texture_2d() failed to launch error = %d\n", error);
    }

	//error = cudaMemcpyFromArray(cudaLinearMemory, cuArray, 0, 0, width * height * 4, cudaMemcpyDeviceToDevice);

	/*error = cudaMemcpy2DToArray(
		cuArray, // dst array
		0, 0,    // offset
		cudaLinearMemory, pitch,       // src
		width * 4 * sizeof(uint8_t), height, // extent
		cudaMemcpyDeviceToDevice);*/
	//getLastCudaError("cudaMemcpy2DToArray failed");

	return cudaLinearMemory;
}