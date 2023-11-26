#include "my_lib.h"
#include <mpi.h>


DisjointSet *firstPass(int *image, int width, int height, int labelOffset, int size) {

    int currentLabel = 1 + labelOffset;
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

void findGlobalEquivalences(DisjointSet *equivalences, int* image, int w, int h, int rows_per_process, int extra_rows) {
    int index, i, j;

    // Cicla attraverso tutte le righe tranne l'ultima
    for (i = 0; i < h - 1; i++) {
        // Determina se la riga corrente è una riga di confine
        int isBoundaryRow = 0;

        // Controlla se la riga corrente è l'ultima riga di un processo
        if (i < (rows_per_process + 1) * extra_rows) {
            // Per i processi con righe extra
            isBoundaryRow = ((i + 1) % (rows_per_process + 1) == 0);
        } else {
            // Per i processi senza righe extra
            isBoundaryRow = ((i - extra_rows) % rows_per_process == rows_per_process - 1);
        }

        // Esegui l'unione solo sulle righe di confine
        if (isBoundaryRow) {
            for (j = 0; j < w; j++) {
                index = i * w + j;
                int nextRowIdx = index + w;

                if (image[index] != 0 && image[nextRowIdx] != 0) {
                    unionSets(equivalences, image[index] - 1, image[nextRowIdx] - 1);
                }
            }
        }
    }
}

#define CREATEIMG 1
int main(int argc, char **argv) {

    MPI_Init(&argc, &argv);
    initializeRandomSeed();
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int w, h;
    int *img = NULL;
    int *recv_buffer = NULL;
    if (rank == 0) {
        img = read_bmp("C:/Path/to/Image/image.bmp", &w, &h);
        #if (CREATEIMG)
        //initializeRandomSeed();
        createColorMappedBMP("color_mapped.bmp", img, w, h);
        #endif
    }

    double start_time = MPI_Wtime();
    MPI_Bcast(&h, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&w, 1, MPI_INT, 0, MPI_COMM_WORLD);
    
    int rows_per_process = h / size;
    int extra_rows = h % size;

    // dim del buffer per ricevere le righe
    int recv_count = (rank < extra_rows) ? (rows_per_process + 1) : rows_per_process;
    recv_buffer = (int *)malloc(recv_count * w * sizeof(int));

    int *send_counts = NULL;
    int *displacements = NULL;

    if (rank == 0) {
        send_counts = (int *)malloc(size * sizeof(int));
        displacements = (int *)malloc(size * sizeof(int));
        int offset = 0;
        for (int i = 0; i < size; i++) {
            send_counts[i] = (i < extra_rows) ? (rows_per_process + 1) * w : rows_per_process * w;
            displacements[i] = offset;
            offset += send_counts[i];
        }
    }

    MPI_Scatterv(img, send_counts, displacements, MPI_INT, recv_buffer, recv_count * w, MPI_INT, 0, MPI_COMM_WORLD);

    /*
    // Risultati intermedi
    #if (CREATEIMG)
        char filename[50];  // Dimensione sufficientemente grande per contenere il nome del file
        sprintf(filename, "color_mapped%d.bmp", rank);
        //stampaMatrice(recv_buffer, w, recv_count);
        createColorMappedBMP(filename, recv_buffer, w, recv_count);
    #endif
    */
    
    int MAX_LABELS_PER_PROCESS = recv_count*w;
    int labelOffset = rank * MAX_LABELS_PER_PROCESS;

    DisjointSet* localEquivalences = firstPass(recv_buffer, w, recv_count, labelOffset, h*w);
    secondPass(recv_buffer, w, recv_count, localEquivalences);
    
    DisjointSet* globEquivalences = NULL;
    if (rank == 0) {
        MPI_Gatherv(recv_buffer, recv_count * w, MPI_INT, img, send_counts, displacements, MPI_INT, 0, MPI_COMM_WORLD);
        globEquivalences = initializeDisjointSet(w * h);
        MPI_Gather(localEquivalences->parent, recv_count * w, MPI_INT, globEquivalences->parent, recv_count * w, MPI_INT, 0, MPI_COMM_WORLD);
        findGlobalEquivalences(globEquivalences, img, w, h, rows_per_process, extra_rows);
        free(localEquivalences->parent);
        free(localEquivalences);
        localEquivalences = globEquivalences;
    } else {
        MPI_Gatherv(recv_buffer, recv_count * w, MPI_INT, img, send_counts, displacements, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Gather(localEquivalences->parent, recv_count * w, MPI_INT, NULL, recv_count * w, MPI_INT, 0, MPI_COMM_WORLD);
    }

    MPI_Bcast(localEquivalences->parent, h*w, MPI_INT, 0, MPI_COMM_WORLD);
    secondPass(recv_buffer, w, recv_count, localEquivalences);
    MPI_Gatherv(recv_buffer, recv_count * w, MPI_INT, img, send_counts, displacements, MPI_INT, 0, MPI_COMM_WORLD);

    // Calcola e stampa il tempo di esecuzione
    double end_time = MPI_Wtime();
    double time_diff = end_time - start_time;
    printf("Processo %d ha impiegato %lf ms.\n", rank, time_diff*1000);

    if (rank == 0) {
        #if (CREATEIMG)
        createColorMappedBMP("color_mapped_final.bmp", img, w, h);
        #endif
    }

    free(localEquivalences->parent);
    free(localEquivalences);
    free(recv_buffer);
    freeHashTable();
    if (rank == 0) {
        free(send_counts);
        free(displacements);
        free(img);
    }

    MPI_Finalize();

    return 0;
}