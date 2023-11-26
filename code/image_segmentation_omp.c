#include "my_lib.h"
#include <omp.h>

// Prima scansione
void firstPass(int *image, int width, int startRow, int finalRow, int labelOffset, int size, DisjointSet *equivalences) {

    int currentLabel = 1 + labelOffset;
    for (int i = startRow; i < finalRow; i++) {
        for (int j = 0; j < width; j++) {
            int index = i * width + j;
            if (image[index] == 1) {
                int above = (i > startRow) ? image[index - width] : 0;
                int left = (j > 0) ? image[index - 1] : 0;
                
                int minNeighbor = (above > 0) ? above : (left > 0) ? left : 0;
                if (minNeighbor == 0) {
                    // Nuova etichetta
                    image[index] = currentLabel;
                    currentLabel++;
                } else {
                    image[index] = minNeighbor;
                    if (above > 0 && left > 0 && above != left) {
                        // Trovata un'equivalenza, unisci gli insiemi
                        unionSets(equivalences, above-1, left-1);
                    }
                }
            }
        }
    }

}

void secondPass(int *image, int width, int startRow, int finalRow, DisjointSet *equivalences) {
    for (int i = startRow; i < finalRow; i++) {
        for (int j = 0; j < width; j++) {
            int index = i * width + j;
            if (image[index] > 0) {
                image[index] = find(equivalences, image[index] - 1) + 1;
            }
        }
    }
}

void findGlobalEquivalences(DisjointSet *equivalences, int* image, int w, int h, int start_row) {
    int index, i, j;
    if (start_row == 0)
        return;
    for (j = 0; j < w; j++) {
        index = start_row * w + j;
        int nextRowIdx = index - w; // Indice della riga precedente
        // Unisci solo se entrambe le celle non sono zero
        if (image[index] != 0 && image[nextRowIdx] != 0) {
            unionSets(equivalences, image[index] - 1, image[nextRowIdx] - 1);
        }
    }
}

#define CREATEIMG 1
int main(int argc, char **argv) {

    initializeRandomSeed();
    int w, h;
    int *img = read_bmp("C:/Path/to/Image/image.bmp", &w, &h);

    #if (CREATEIMG)
    createColorMappedBMP("color_mapped.bmp", img, w, h);
    #endif

    int n_threads;
    if (argc < 2) {
        // Nessun argomento fornito, usa il numero di thread di default
        n_threads = omp_get_max_threads();
        printf("Utilizzo del numero di thread di default: %d\n", n_threads);

    } else {
        // Argomento fornito, usa quel numero di thread
        n_threads = atoi(argv[1]);
        if (n_threads <= 0) {
            printf("Inserire un numero valido di thread. Utilizzo del numero di thread di default.\n");
            n_threads = omp_get_max_threads();
        }
        else 
            printf("Utilizzo del numero di thread richiesto: %d\n", n_threads);
    }

    double start_time = omp_get_wtime();
    int rows_per_thread = h / n_threads;
    int extra_rows = h % n_threads;
    int MAX_LABELS_PER_THREAD = (rows_per_thread+1) * w;
    DisjointSet *equivalences = initializeDisjointSet(h*w);
    
    #pragma omp parallel num_threads(n_threads)
    {
        int thread_id = omp_get_thread_num();
        int start_row, end_row;
         if (thread_id < extra_rows) {
            // I primi 'extra_rows' thread processano 'rows_per_thread + 1' righe
            start_row = thread_id * (rows_per_thread + 1);
            end_row = start_row + (rows_per_thread + 1);
        } else {
            // I rimanenti thread processano 'rows_per_thread' righe
            start_row = thread_id * rows_per_thread + extra_rows;
            end_row = start_row + rows_per_thread;
        }

        int labelOffset = thread_id * MAX_LABELS_PER_THREAD;

        firstPass(img, w, start_row, end_row, labelOffset, h * w,equivalences);
        secondPass(img, w, start_row, end_row, equivalences);

        findGlobalEquivalences(equivalences, img, w, h, start_row);

        secondPass(img, w, start_row, end_row, equivalences);

    }

    double end_time = omp_get_wtime();
    printf("Tempo impiegato: %lf ms.\n", (end_time - start_time)*1000);

    free(equivalences->parent);
    free(equivalences);

    #if (CREATEIMG)
    createColorMappedBMP("color_mapped_final.bmp", img, w, h);
    #endif

    free(img);
    freeHashTable();

    return 0;
}