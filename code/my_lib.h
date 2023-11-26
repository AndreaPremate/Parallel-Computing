#ifndef MYLIB_H
#define MYLIB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#define MAX_COLOR_MAPPINGS 4096*4096/2      //numero massimo di CC nell'immagine più grande analizzata(modello a scacchiera)

struct ColorMapping {
    int number;
    uint32_t color;
    struct ColorMapping *next;
};

typedef struct {
    int *parent;
} DisjointSet;

extern struct ColorMapping *hashTable[MAX_COLOR_MAPPINGS];

// Dichiarazioni funzioni
int *read_bmp(char *fname, int *_w, int *_h);
void freeHashTable();
void initializeRandomSeed();
uint32_t generateRandomColor();
unsigned int hashFunction(int number);
void insertColorMapping(int number, uint32_t color);
uint32_t mapValueToColor(int number);
void createColorMappedBMP(const char *filename, int *matrix, int width, int height);
void stampaMatrice(int *mat, int h, int w);

DisjointSet *initializeDisjointSet(int size);
int find(DisjointSet *set, int element);
void unionSets(DisjointSet *set, int element1, int element2);
void printDisjointSet(DisjointSet *set, int size);

#ifdef __cplusplus
}
#endif

#endif // MYLIB_H
