#include "my_lib.h"

// Prima scansione
DisjointSet *firstPass(int *image, int width, int height, int size) {

    int currentLabel = 1;
    DisjointSet *equivalences = initializeDisjointSet(size);
    
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            int index = i * width + j;
            if (image[index] == 1) {
                int above = (i > 0) ? image[index - width] : 0;
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

	return equivalences;
}

void secondPass(int *image, int width, int height, DisjointSet *equivalences) {
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            int index = i * width + j;
            if (image[index] > 0) {
                image[index] = find(equivalences, image[index] - 1) + 1;
            }
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

    clock_t start = clock();

    DisjointSet* equivalences = firstPass(img, w, h, w*h);
    secondPass(img, w, h, equivalences);

    clock_t end = clock();
    double tempo_trascorso = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Tempo di esecuzione: %lf ms", tempo_trascorso*1000);

    free(equivalences->parent);
    free(equivalences);

    #if (CREATEIMG)
    createColorMappedBMP("color_mapped_final.bmp", img, w, h);
    #endif

    free(img);
    freeHashTable();

    return 0;
}