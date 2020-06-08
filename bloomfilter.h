#ifndef __BLOOM__
#define __BLOOM__

#include <stdlib.h>
#include <stdint.h>

#define KEYT uint32_t
typedef struct{
	uint32_t k;
	int m;
	int targetsize;
	int n;
	float p;
	char *body;
}BF;


// Fibonacci hash
static inline uint32_t hashing_key(uint32_t key) {
    return (uint32_t)((0.618033887 * key) * 1024);
}

static inline KEYT hashfunction(KEYT key) {
	
    key ^= key >> 15;
	key *= 2246822519U;
	key ^= key >> 13;
	key *= 3266489917U;
	key ^= key >> 16;
	return key;
//    return hashing_key(key);
}


BF* bf_init(int entry, float fpr);
bool bf_check(BF* input, KEYT key);
void bf_free(BF* input);
void bf_set(BF *input, KEYT key);

uint64_t bf_bits(int entry, float fpr);


#endif
