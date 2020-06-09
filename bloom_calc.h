#ifndef __BLOOM_CALC_H__
#define __BLOOM_CALC_H__
#include "device.h"
#include "bloomfilter.h"
#include <stdint.h>
#include <queue>
#include <deque>
#define GCONEBLOCK
enum{
USER, REBLOOM, GC
};

#define FPR (0.00041f)
inline bool set_bitmap(uint8_t *bitmap, uint32_t idx){
	uint32_t offset=idx/8;
	uint32_t remain=idx%8;
	bool res=true;
	if(bitmap[offset] & 1<<remain){
		res=false;
	}
	bitmap[offset]|=1<<remain;
	return res;
}

inline void unset_bitmap(uint8_t *bitmap, uint32_t idx){
	uint32_t offset=idx/8;
	uint32_t remain=idx%8;
	bitmap[offset]&=~(1<<remain);
}

inline bool check_bitmap(uint8_t *bitmap, uint32_t idx){
	uint32_t offset=idx/8;
	uint32_t remain=idx%8;
	if(bitmap[offset] &1<<remain){
		return true;
	}
	return false;
}

typedef struct bloom_argument{
	uint64_t TOTAL_DEV_SIZE;
	uint64_t PS; //page size
	uint64_t PPB; //page num per block
	uint64_t BPS; //block per super block
	float target_fpr;
	uint32_t filter_additional_space;

	uint64_t NUM_TOTAL_BLOCK;
	uint64_t NUM_SUP_BLOCK;//# of super_block 
	uint32_t LPP; //logical page per physical page
	uint32_t lba_shift; // shift bits for find target block;

	float bloom_fpr; // not implemented
	uint64_t needed_memory; //not implemented
}bloom_arg;

typedef struct physical_page{
	bool isfirst;
	//uint32_t invalid;
	uint8_t invalid_bit;
	uint32_t invalid_num;
	uint32_t *oob; 
	BF **filter_array;
}p_page;

typedef struct block{
	uint32_t idx;
	uint32_t used;
	uint32_t invalid;
	uint8_t *ppa_valid_map;	

	uint32_t now_buffer_ptr;
	uint32_t *lba_buffer;

	uint32_t now_page_ptr;
	p_page *page_array;
}block;

typedef struct super_block{
	uint32_t idx;
	uint64_t used;
	uint64_t filter_num;
	block *now;
	std::queue<block *> *free_block_set;
	std::deque<block *> *used_block_set;
}sup_block;

typedef struct bloom_ftl{
	block *block_super_set;
	sup_block *sb_array;
	//uint8_t *lba_valid_map;
	uint8_t *lba_length_map;
	uint32_t read;
	uint32_t read_error;
	uint32_t write;
	uint32_t gc_read;
	uint32_t gc_write;
	uint32_t rb_read;
	uint32_t rb_write;
	uint32_t invalid_num;
	uint32_t gc_num;
	uint32_t trim;
}bloom_ftl;

uint32_t block_free(block *);
uint32_t block_init(block *, uint32_t idx);

uint32_t bff_init(int argc, char **argv);
uint32_t bff_write(uint32_t lba);
uint32_t bff_read(uint32_t lba);
uint32_t bff_free();
uint32_t gc(sup_block *target);
uint32_t rebloom(sup_block *target);
uint32_t sp_write(sup_block *target, uint32_t lba, uint32_t type );

uint32_t block_print(block *target);
uint32_t block_filter_num(block *target);
bool block_check(block *target);
#endif
