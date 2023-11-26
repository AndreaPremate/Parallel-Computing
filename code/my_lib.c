#include "my_lib.h"

///////////////////////////////////////////
/////////// LETTURA E SCRITTURA ///////////
///////////////////////////////////////////
   
int *read_bmp(char *fname, int *_w, int *_h) {
    unsigned char head[54];
    FILE *f = fopen(fname, "rb");

    // BMP header
    fread(head, 1, 54, f);
    int w = head[18] + (((int)head[19]) << 8) + (((int)head[20]) << 16) + (((int)head[21]) << 24);
    int h = head[22] + (((int)head[23]) << 8) + (((int)head[24]) << 16) + (((int)head[25]) << 24);

    // lines are aligned on 4-byte boundary
    int lineSize = (w / 8 + (w / 8) % 4);
    int fileSize = lineSize * h;

    unsigned char *data = (unsigned char *) malloc(fileSize);

    // skip header
    fseek(f, 54, SEEK_SET);
    // skip palette - two rgb quads, 8 bytes
    fseek(f, 8, SEEK_CUR);

    // read data
    fread(data, 1, fileSize, f);
    // decode bits
    int i, byte_ctr, j, rev_j;
    int *img = (int*) malloc(w * h * sizeof(int));
    for (j = 0, rev_j = h - 1; j < h; j++, rev_j--) {
        for (i = 0; i < w; i++) {
            byte_ctr = i / 8;
            unsigned char data_byte = data[j * lineSize + byte_ctr];
            int pos = rev_j * w + i;
            unsigned char mask = 0x80 >> i % 8;
            img[pos] = (data_byte & mask) ? 1 : 0;
        }
    }

    free(data);
    *_w = w;
    *_h = h;

    return img;
}

struct ColorMapping *hashTable[MAX_COLOR_MAPPINGS] = { NULL };

void freeHashTable() {
    for (int i = 0; i < MAX_COLOR_MAPPINGS; i++) {
        struct ColorMapping *current = hashTable[i];
        while (current != NULL) {
            struct ColorMapping *temp = current;
            current = current->next;
            free(temp);
        }
    }
}

void initializeRandomSeed() {
    srand(time(NULL));
}

uint32_t generateRandomColor() {
    uint8_t r = rand() % 256;
    uint8_t g = rand() % 256;
    uint8_t b = rand() % 256;
    return (r << 16) | (g << 8) | b;
}

unsigned int hashFunction(int number) {
    return number % MAX_COLOR_MAPPINGS;
}

void insertColorMapping(int number, uint32_t color) {
    unsigned int index = hashFunction(number);
    struct ColorMapping *newEntry = malloc(sizeof(struct ColorMapping));
    newEntry->number = number;
    newEntry->color = color;
    newEntry->next = hashTable[index];
    hashTable[index] = newEntry;
}

uint32_t mapValueToColor(int number) {
    unsigned int index = hashFunction(number);
    struct ColorMapping *entry = hashTable[index];

    while (entry != NULL) {
        if (entry->number == number) {
            return entry->color;
        }
        entry = entry->next;
    }

    // Se non trovato, inserisci un nuovo colore
    uint32_t newColor = generateRandomColor();
    insertColorMapping(number, newColor);
    return newColor;
}

void createColorMappedBMP(const char *filename, int *matrix, int width, int height) {
    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("Errore nell'apertura del file");
        return;
    }

    int bytes_per_row = width * 3;
    int padding = (4 - (bytes_per_row % 4)) % 4;

    // header BMP
    uint8_t bmp_header[54] = {
        0x42, 0x4D, 0x36, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00,
        0x28, 0x00, 0x00, 0x00,
        width & 0xFF, (width >> 8) & 0xFF, (width >> 16) & 0xFF, (width >> 24) & 0xFF,
        height & 0xFF, (height >> 8) & 0xFF, (height >> 16) & 0xFF, (height >> 24) & 0xFF,
        0x01, 0x00, 0x18, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x13, 0x0B, 0x00, 0x00,
        0x13, 0x0B, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    };

    fwrite(bmp_header, sizeof(uint8_t), 54, file);

    // Scrivi dati dell'immagine
    for (int i = height - 1; i >= 0; i--) {
        for (int j = 0; j < width; j++) {
            int value = matrix[i * width + j];
            uint32_t color;
            if (value == 0) {
                color = 0x000000; // Imposta il colore nero per il valore 0
            } else {
                color = mapValueToColor(value);
            }

            uint8_t pixel[3];
            pixel[0] = (color >> 16) & 0xFF; // Red
            pixel[1] = (color >> 8) & 0xFF;  // Green
            pixel[2] = color & 0xFF;         // Blue
            fwrite(pixel, sizeof(uint8_t), 3, file);
        }
        for (int k = 0; k < padding; k++) {
            fputc(0x00, file); // Aggiungi il padding alla fine di ciascuna riga
        }
    }
    fclose(file);
}

void stampaMatrice(int *mat, int h, int w) {
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            int element = mat[i * w + j];
            printf("%*d ", 4, element);
        }
        printf("\n");
    }
}


///////////////////////////////////////////
////////////// DISJOINTSET ////////////////
///////////////////////////////////////////

DisjointSet *initializeDisjointSet(int size) {
    DisjointSet *set = (DisjointSet *)malloc(sizeof(DisjointSet));
    set->parent = (int *)malloc(size * sizeof(int));
    
    for (int i = 0; i < size; i++) {
        set->parent[i] = -1; 
    }
    
    return set;
}

// Trova il rappresentante di un elemento
int find(DisjointSet *set, int element) {
    // Trova la radice dell'elemento
    int root = element;
    while (set->parent[root] >= 0) {
        root = set->parent[root];
    }

    // compressione percorso
    while (element != root) {
        int next = set->parent[element];
        set->parent[element] = root;
        element = next;
    }

    return root;
}

void unionSets(DisjointSet *set, int element1, int element2) {
    int root1 = find(set, element1);
    int root2 = find(set, element2);
    
    if (root1 != root2) {
        set->parent[root1] = root2;
    }
}

void printDisjointSet(DisjointSet *set, int size) {
    if (set == NULL || set->parent == NULL) {
        printf("L'insieme è vuoto o non inizializzato.\n");
        return;
    }

    printf("Elementi dell'insieme disgiunto:\n");
    for (int i = 0; i < size; i++) {
        printf("Elemento %d: Parent = %d\n", i, set->parent[i]);
    }
}
