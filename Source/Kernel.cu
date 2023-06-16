#include "Pch.h"
#include <cuda_runtime.h>
#include "device_launch_parameters.h"
#include <math.h>
#include <stdint.h>

#include <Windows.h>

#define R 1         //filter radius
#define D R*1 + 1   //filter diameter
#define S D*D       //filter size
#define TILE 16
#define BLOCK_W 16+(2*R)
#define BLOCK_H 16+(2*R)

__global__ void kernel(const uint8_t* pixels, uint8_t* out, int width, int height, int comp)
{
    const int SOBEL_X[] = {-1, 0, 1, -2, 0, 2, -1, 0, 1};
    const int SOBEL_Y[] = {-1, -2, -1, 0, 0, 0, 1, 2, 1};

    int x = threadIdx.x + blockIdx.x * blockDim.x;
    int y = threadIdx.y + blockIdx.y * blockDim.y;
    int index = y * width + x;

    float LuminanceConv[3] = {0.2125f, 0.7154f, 0.0721f};

    if ((x >= 1) && (x < width - 1) && (y >= 1) && (y < height - 1)) {
        float axr = 0, axg = 0, axb = 0;
        float ayr = 0, ayg = 0, ayb = 0;
        int ix = 0;
        
        for (int i = -1; i <= 1; i++) {
            for (int j = -1; j <= 1; j++) {

                ix = pixels[3 * ((y + i) * width + x + j)];
                axr += ix * SOBEL_X[(i + 1) * 3 + j + 1];
                ayr += ix * SOBEL_Y[(i + 1) * 3 + j + 1];
                ix = pixels[3 * ((y + i) * width + x + j) + 1];
                axg += ix * SOBEL_X[(i + 1) * 3 + j + 1];
                ayg += ix * SOBEL_Y[(i + 1) * 3 + j + 1];
                ix = pixels[3 * ((y + i) * width + x + j) + 2];
                axb += ix * SOBEL_X[(i + 1) * 3 + j + 1];
                ayb += ix * SOBEL_Y[(i + 1) * 3 + j + 1];
            }
        }

        axr *= LuminanceConv[0];
        axg *= LuminanceConv[1];
        axb *= LuminanceConv[2];
        ayr *= LuminanceConv[0];
        ayg *= LuminanceConv[1];
        ayb *= LuminanceConv[2];

        out[3 * index] = max(min(sqrt((axr * axr) + (ayr * ayr)), 255.0f), 0.0f);
        out[3 * index + 1] = max(min(sqrt((axg * axg) + (ayg * ayg)), 255.0f), 0.0f);
        out[3 * index + 2] = max(min(sqrt((axb * axb) + (ayb * ayb)), 255.0f), 0.0f);
    }

}

void processImage(uint8_t* pixels, int width, int height, int comp)
{
    int size = width * height * comp * sizeof(uint8_t);

    #if 0 // breakpoint
    while (!IsDebuggerPresent())
        ;
    #endif

    void* ptr;
    cudaError r = cudaMalloc(&ptr, size);
    void* out;
    r = cudaMalloc(&out, size);
    r = cudaMemcpy(ptr, pixels, size, cudaMemcpyHostToDevice);

    dim3 blocks(16, 16, 1);
    dim3 grid(1, 1, 1);
    grid.x = (width + blocks.x - 1) / blocks.x;
    grid.y = (height + blocks.y - 1) / blocks.y;

    kernel<<<grid, blocks>>>((uint8_t*)ptr, (uint8_t*)out, width, height, comp);

    //r = cudaDeviceSynchronize();
    r = cudaMemcpy(pixels, out, size, cudaMemcpyDeviceToHost);
    r = cudaFree(ptr);
    r = cudaFree(out);
}
