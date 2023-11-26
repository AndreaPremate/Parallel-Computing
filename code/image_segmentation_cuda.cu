#include "my_lib.h"
#include <limits.h>


int* add_padding(int* original, int cols, int rows) {

    int padded_rows = rows + 2;
    int padded_cols = cols + 2;
    int size_with_padding = (padded_rows) * (padded_cols);

    int* padded_matrix = (int*)malloc(size_with_padding * sizeof(int));

    // Inizializza la matrice con padding a zero
    memset(padded_matrix, 0, size_with_padding * sizeof(int));

    // Copia i valori
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            padded_matrix[(i + 1) * (padded_cols) + (j + 1)] = original[i * cols + j];
        }
    }

    return padded_matrix;
}

int* remove_padding(int* padded_matrix, int padded_cols, int padded_rows) {

    int original_rows = padded_rows - 2;
    int original_cols = padded_cols - 2;

    int* original_matrix = (int*)malloc(original_rows * original_cols * sizeof(int));
 
    // Copia i valori
    for (int i = 0; i < original_rows; ++i) {
        for (int j = 0; j < original_cols; ++j) {
            original_matrix[i * original_cols + j] = padded_matrix[(i + 1) * padded_cols + (j + 1)];
        }
    }

    return original_matrix;
}



__global__ void InitLabels(int* Labels, int SIZEX) {
    int SIZEXPAD = SIZEX + 2;
    int id = blockIdx.y * gridDim.x * blockDim.x + blockIdx.x * blockDim.x + threadIdx.x;
    int cy = id / SIZEX;
    int cx = id - cy * SIZEX;
    int aPos = (cy + 1) * SIZEXPAD + cx + 1;
    int l = Labels[aPos];
    l *= aPos;
    Labels[aPos] = l;
}

__global__ void Scanning(int* Labels, int* IsNotDone, int SIZEX, int SIZEY) {
    int SIZEYPAD = SIZEY + 2;
    int SIZEXPAD = SIZEX + 2;
    unsigned int id = blockIdx.y * gridDim.x * blockDim.x + blockIdx.x * blockDim.x + threadIdx.x;
    unsigned int cy = id / SIZEX;
    unsigned int cx = id % SIZEX;
    unsigned int aPos = (cy + 1) * SIZEXPAD + cx + 1;

    if (aPos < SIZEXPAD * SIZEYPAD) { 
        unsigned int l = Labels[aPos];
        if (l) {
            unsigned int lw = Labels[aPos - 1];
            unsigned int le = Labels[aPos + 1];
            unsigned int ls = Labels[aPos - SIZEX - 2];
            unsigned int ln = Labels[aPos + SIZEX + 2];
            unsigned int minl = INT_MAX;

            if (lw) minl = lw;
            if (le && le < minl) minl = le;
            if (ls && ls < minl) minl = ls;
            if (ln && ln < minl) minl = ln;

            if (minl < l) {
                unsigned int ll = Labels[l];
                Labels[l] = min(ll, minl);
                IsNotDone[0] = 1;
            }
        }
    }
}

__global__ void Analysis(int* Labels, int SIZEX, int SIZEY) {
    int SIZEYPAD = SIZEY + 2;
    int SIZEXPAD = SIZEX + 2;
    unsigned int id = blockIdx.y * gridDim.x * blockDim.x + blockIdx.x * blockDim.x + threadIdx.x;

    unsigned int cy = id / SIZEX;
    unsigned int cx = id % SIZEX;
    unsigned int aPos = (cy + 1) * SIZEXPAD + cx + 1;

    if (aPos < SIZEXPAD * SIZEYPAD) { 
        unsigned int label = Labels[aPos];
        if (label) {
            unsigned int r = Labels[label];
            while (r != label) {
                label = Labels[r];
                r = Labels[label];
            }
            Labels[aPos] = label;
        }
    }
}


#define CREATEIMG 1
int main(int argc, char **argv) {

    initializeRandomSeed();
    int w, h;
    int *h_img_no_padding = read_bmp("C:/Path/to/Image/image.bmp", &w, &h);
    int *h_img = add_padding(h_img_no_padding, w, h);      
    #if (CREATEIMG)
    createColorMappedBMP("color_mapped.bmp", h_img, w+2, h+2);
    #endif
    int img_size = (w+2)*(h+2);
    
    // START TIMER
    cudaEvent_t start, stop;
    cudaEventCreate(&start);
    cudaEventCreate(&stop);
    cudaEventRecord(start);

    // Allocazione della memoria sul device
    int* d_img;
    cudaMalloc((void**)&d_img, img_size * sizeof(int));

    // Copia dei dati dall'host al device
    cudaMemcpy(d_img, h_img, img_size * sizeof(int), cudaMemcpyHostToDevice);

    dim3 threadsPerBlock(64);
    dim3 blocksPerGrid((w+2 + threadsPerBlock.x - 1) / threadsPerBlock.x,
                   (h+2 + threadsPerBlock.y - 1) / threadsPerBlock.y);

    // Allocazione e inizializzazione di IsNotDone
    int *d_isNotDone;
    int h_isNotDone = 1; // 1 per entrare nel ciclo
    cudaMalloc((void**)&d_isNotDone, sizeof(int));
    cudaMemcpy(d_isNotDone, &h_isNotDone, sizeof(int), cudaMemcpyHostToDevice);

    // Inizializza labels
    InitLabels<<<blocksPerGrid, threadsPerBlock>>>(d_img, w);

    // Ciclo Scanning e Analysis
    do {
        h_isNotDone = 0; 
        cudaMemcpy(d_isNotDone, &h_isNotDone, sizeof(int), cudaMemcpyHostToDevice);

        Scanning<<<blocksPerGrid, threadsPerBlock>>>(d_img, d_isNotDone, w, h);
        Analysis<<<blocksPerGrid, threadsPerBlock>>>(d_img, w, h);

        cudaMemcpy(&h_isNotDone, d_isNotDone, sizeof(int), cudaMemcpyDeviceToHost);
    } while (h_isNotDone);

    cudaEventRecord(stop);
    cudaEventSynchronize(stop);

    // Calcolo e stampa del tempo trascorso
    float milliseconds = 0;
    cudaEventElapsedTime(&milliseconds, start, stop);
    printf("Tempo di esecuzione: %f ms\n", milliseconds);

    // Pulizia eventi CUDA
    cudaEventDestroy(start);
    cudaEventDestroy(stop);
    
    // Copia dei risultati indietro sull'host
    cudaMemcpy(h_img, d_img, img_size * sizeof(int), cudaMemcpyDeviceToHost);
    h_img_no_padding = remove_padding(h_img, w+2, h+2);
    #if (CREATEIMG)
    createColorMappedBMP("color_mapped_final.bmp", h_img_no_padding, w, h);
    #endif

    // Libera memoria
    free(h_img);
    free(h_img_no_padding);
    freeHashTable();
    cudaFree(d_img);

    return 0;
}