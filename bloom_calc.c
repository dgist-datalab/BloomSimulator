#include "bloom_calc.h"
#include<string.h>
#include<stdlib.h>
#include<getopt.h>
#include <stdio.h>
#include<math.h>
bloom_arg arg;
bloom_ftl ftl;

uint32_t page_init(p_page *t, bool isfirst){
	t->isfirst=isfirst;
	t->oob=(uint32_t*)malloc(arg.LPP * sizeof(uint32_t ));
	t->filter_array=(BF**)malloc(arg.LPP * sizeof(BF*));
	memset(t->filter_array, 0, arg.LPP * sizeof(BF*));
	t->invalid_bit=0;
	t->invalid_num=0;
	/*
	for(uint32_t i=0; i<arg.LPP; i++){
		t->filter_array[i]=bf_init(1, arg.bloom_fpr);
	}*/
	return 1;
}

uint32_t page_free(p_page *t){
	for(int i=0; i<arg.LPP; i++){
		if(t->filter_array[i]) bf_free(t->filter_array[i]);
	}
	free(t->filter_array);
	free(t->oob);
	return 1;
}

uint32_t block_init(block *target, uint32_t idx){
	memset(target, 0, sizeof(block));
	target->ppa_valid_map=(uint8_t*)malloc(arg.PPB/8);
	memset(target->ppa_valid_map, 0, arg.PPB/8);

	target->lba_buffer=(uint32_t*)malloc(arg.LPP * sizeof(uint32_t));
	memset(target->lba_buffer, 0, arg.LPP * sizeof(uint32_t));

	target->page_array=(p_page*)malloc(sizeof(p_page) * arg.PPB);
	for(uint32_t i=0; i<arg.PPB; i++){
		page_init(&target->page_array[i], i==0);
	}
	target->idx=idx;

	return 1;
}

uint32_t block_free(block *target){
	free(target->ppa_valid_map);
	free(target->lba_buffer);

	for(uint32_t i=0; i<arg.PPB; i++){
		page_free(&target->page_array[i]);
	}

	free(target->page_array);
	return 1;
}

uint32_t sup_block_init(sup_block *target, uint32_t idx){
	memset(target, 0, sizeof(sup_block));
	target->free_block_set=new std::queue<block*>();
	target->used_block_set=new std::deque<block*>();

	for(uint32_t i=idx; i<idx+arg.BPS; i++){
		block *member=&ftl.block_super_set[i];
		block_init(member, i);
		target->free_block_set->push(member);
	}

	target->now=target->free_block_set->front();
	target->free_block_set->pop();
	return 1;
}

uint32_t sup_block_free(sup_block *target){
	delete target->free_block_set;
	delete target->used_block_set;
	return 1;
}


uint32_t bff_init(int argc, char **argv){
	/*init arg*/
	int c;
	memset(&arg, 0 , sizeof(bloom_arg));
	bool arg_set[6];
	memset(arg_set, 0, sizeof(arg_set));
	/*
		d	device size [GB]
		p	page size
		b	PPB
		f	each bloomfilter's fpr
		s	filter_additional_space;
		S	BPS
	 */
	while((c=getopt(argc, argv, "dpbfsS"))!=-1){
		switch(c){
			case 'd':
				arg.TOTAL_DEV_SIZE=atoi(argv[optind]) * GiB;
				arg_set[0]=true;
				break;
			case 'p':
				arg.PS=atoi(argv[optind]);
				arg_set[1]=true;
				break;
			case 'b':
				arg.PPB=atoi(argv[optind]);
				arg_set[2]=true;
				break;
			case 'f':
				arg.bloom_fpr=(float)atof(argv[optind]);
				arg_set[3]=true;
				break;
			case 's':
				arg.filter_additional_space=atoi(argv[optind]);
				arg_set[4]=true;
				break;
			case 'S':
				arg.BPS=atoi(argv[optind]);
				arg_set[5]=true;
				break;
		}
	}

	if(!arg_set[0]) arg.TOTAL_DEV_SIZE=DEFTOTALSIZE;
	if(!arg_set[1]) arg.PS=DEFPAGESIZE;
	arg.LPP=arg.PS/LBASIZE;

	if(!arg_set[2]) arg.PPB=DEFBLOCKPERPAGE;
	if(!arg_set[3]) arg.bloom_fpr=FPR;
	if(!arg_set[5]) arg.BPS=DEFSUPERBLOCKPERBLOCK;

	if(!arg_set[4]) arg.filter_additional_space=(arg.LPP * arg.PPB * arg.BPS )/5;

	arg.NUM_TOTAL_BLOCK=arg.TOTAL_DEV_SIZE/arg.PS/arg.PPB;
	arg.NUM_SUP_BLOCK=arg.NUM_TOTAL_BLOCK/arg.BPS;
	arg.lba_shift=log2(arg.LPP);


	printf("TOTAL_DEV_SIZE: %lu\n",arg.TOTAL_DEV_SIZE);
	printf("PAGE_SIZE: %lu\n",arg.PS);
	printf("PAGE_PER_BLOCK: %lu\n",arg.PPB);
	printf("BLOCK_PER_SUPER: %lu\n",arg.BPS);
	printf("filter_addtional_space: %u\n",arg.filter_additional_space);

	printf("TOTAL_BLOCK: %lu\n",arg.NUM_TOTAL_BLOCK);
	printf("TOTAL_SUP_BLOCK: %lu\n",arg.NUM_SUP_BLOCK);
	printf("logical block per physical :%u (%u)\n", arg.LPP, arg.lba_shift);

	printf("bloom fpr:%lf\n", arg.bloom_fpr);
	


	//arg.bloom_fpr=FPR;

	//arg.needed_memory=6*arg.TOTAL_DEV_SIZE/LBASIZE/8;

	/*bloom FTL init*/
	memset(&ftl, 0, sizeof(bloom_ftl));
	ftl.block_super_set=(block*)malloc(sizeof(block) * arg.NUM_TOTAL_BLOCK);
	ftl.sb_array=(sup_block*)malloc(sizeof(sup_block) * arg.NUM_SUP_BLOCK);
	for(uint32_t i=0; i<arg.NUM_SUP_BLOCK; i++){
		sup_block_init(&ftl.sb_array[i], i * arg.BPS);
		ftl.sb_array[i].idx = i;
	}
	
	ftl.lba_length_map=(uint8_t*)malloc(arg.TOTAL_DEV_SIZE/LBASIZE);
	memset(ftl.lba_length_map,0,arg.TOTAL_DEV_SIZE/LBASIZE);
	return 1;
}

uint32_t bff_free(){
	for(uint32_t i=0; i<arg.NUM_TOTAL_BLOCK; i++){
		block_free(&ftl.block_super_set[i]);
	} 

	for(uint32_t i=0; i<arg.NUM_SUP_BLOCK; i++){
		sup_block_free(&ftl.sb_array[i]);
	}
	
	printf("read:%u\n",ftl.read);
	printf("read_error:%u\n",ftl.read_error);
	printf("write:%u\n",ftl.write);
	printf("gc_read:%u\n",ftl.gc_read);
	printf("gc_write:%u\n",ftl.gc_write);
	printf("rb_read:%u\n",ftl.rb_read);
	printf("rb_write:%u\n",ftl.rb_write);
	printf("trim:%u\n",ftl.trim);

	free(ftl.block_super_set);
	free(ftl.sb_array);
	free(ftl.lba_length_map);
	return 1;
}

enum write_status{
	NOFILTER, NEWFILTER, FULL
};

uint32_t page_write(p_page *target, uint32_t *lba_array, uint32_t type, bool isfirst){
	/*sorting buffer*/
	for(uint32_t i=0; i<arg.LPP-1; i++){
		for(uint32_t j=i+1; j<arg.LPP; j++){
			if(lba_array[i]> lba_array[j]){
				uint32_t temp=lba_array[i];
				lba_array[i]=lba_array[j];
				lba_array[j]=temp;
			}
		}
	}

	memcpy(target->oob, lba_array, arg.LPP * sizeof(uint32_t));
	uint32_t filter_num=0;
	/*bloom filter set*/
	for(uint32_t i=0; i<arg.LPP;){
		uint32_t skip=1;
		if(i!=arg.LPP-1){
			while(i+skip <arg.LPP && lba_array[i]+skip == lba_array[i+skip]) skip++;
		}

		if(target->filter_array[i]){
			printf("filter must be empty!\n");
			abort();
		}
		if(skip>1){
			//printf("break!\n");
		}
		target->filter_array[i]=bf_init(1, arg.bloom_fpr);
		filter_num++;
		
		for(uint32_t j=0; j<skip; j++){
			ftl.lba_length_map[lba_array[i+j]]=j;
		}

		bf_set(target->filter_array[i], lba_array[i]);
		i+=skip;
	}

	switch(type){
		case USER:
			ftl.write++;
			break;
		case REBLOOM:
			ftl.rb_write++;
			break;
		case GC:
			ftl.gc_write++;
			break;
	}
	return filter_num;
}

uint32_t block_buffer_flush(block *target, uint32_t type){
	uint32_t filter_num=page_write(&target->page_array[target->now_page_ptr], target->lba_buffer, type, 
			target->now_page_ptr==0);
	set_bitmap(target->ppa_valid_map, target->now_page_ptr);
	target->now_page_ptr++;
	target->now_buffer_ptr=0;
	target->used++;
	return filter_num;
}

uint32_t block_write(block *target, uint32_t lba, uint32_t *filter_num, uint32_t type){
	uint32_t res=NOFILTER;
	/*
	if(lba==655360){
		printf("break!\n");
	}
	*/

	if(target->now_buffer_ptr >= arg.LPP){
		(*filter_num)=block_buffer_flush(target,type);
		if(target->used == arg.PPB){
			res=FULL;
			return res;
		}
		else if(target->used > arg.PPB){
			printf("block over!!\n");
			abort();
		}
		else{
			res=NEWFILTER;
		}
	}
		
	target->lba_buffer[target->now_buffer_ptr++]=lba;

	return res;
}

uint32_t invalid_num_sup(sup_block *target){
	uint32_t res=0;
	for(uint32_t i=target->idx*arg.BPS; i<target->idx*arg.BPS+arg.BPS; i++){
		res+=ftl.block_super_set[i].invalid;
	}
	return res;
}

extern bool debug;
uint32_t sp_write(sup_block *target, uint32_t lba, uint32_t type ){
	uint32_t new_filter_num;
	uint32_t status;
retry:
	if(target->idx==0){
		//printf("lba:%u\n",lba);
	}
	
	if(target->used == arg.PPB * arg.BPS){
		if(type==REBLOOM){
			printf("GC triggered while Rebloom break!\n");
	//		debug=true;
		}
		if(invalid_num_sup(target)==0){
			rebloom(target);
			goto retry;
		}
		else{
			gc(target);
		}
		if(type==REBLOOM){
	//		debug=false;
		}
	}

	if(target->filter_num > (arg.PPB * arg.BPS * arg.LPP /2) + arg.filter_additional_space){

		if(target->now->now_buffer_ptr >= arg.LPP){
			if(type!=USER){
				printf("rebloom triggered by not user REQ\n");
				abort() ;
			}
			target->filter_num+=block_buffer_flush(target->now,USER);
			target->used++;
			if(target->now->used == arg.PPB){
				if(target->free_block_set->size()){
					target->now=target->free_block_set->front();
					target->free_block_set->pop();
					rebloom(target);
				}
				else{
					target->used_block_set->push_back(target->now);
					target->now=NULL;
					printf("rebloom triggered but trigger GC\n");
					gc(target);
				}
			}
			else{
				rebloom(target);	
			}
		}
	}

	if(!target->now){
		printf("break!\n");
	}
	status=block_write(target->now, lba, &new_filter_num, type);
	switch(status){
		case NOFILTER:
			break;
		case NEWFILTER:
			target->filter_num+=new_filter_num;
			target->used++;
			break;
		case FULL:
			target->filter_num+=new_filter_num;
			target->used_block_set->push_back(target->now);
			if(target->free_block_set->size()){
				target->now=target->free_block_set->front();
				target->free_block_set->pop();
			}
			else{
				target->now=NULL;
			}
			target->used++;
			goto retry;
	}
	return 1;
}

uint32_t bff_write(uint32_t lba){
	uint32_t super_block_idx=(lba>>arg.lba_shift)%arg.NUM_SUP_BLOCK;
	//printf("super_block_idx :%u\n", super_block_idx);
	return sp_write(&ftl.sb_array[super_block_idx], lba, USER);
}

uint32_t page_read(p_page *target, uint32_t check_lba, uint32_t lba, bool isfirst){
	uint32_t bf_max=arg.LPP;
	bool isread=false;
	for(uint32_t i=0; i<bf_max; i++){
		if(target->filter_array[i]){
			/*bf_check*/
			if(bf_check(target->filter_array[i], check_lba)){
				/*if true, set isread true*/
				isread=true;
				break;
			}
		}
	}

	if(isread || isfirst){
		ftl.read++;
		bool test=false;

		for(uint32_t i=0; i<bf_max; i++){
			if(target->oob[i]==lba){
				test=true;
				break;
			}
		}

		if(isfirst && !test){
			printf("check_lba:%u lba:%u pass bloom filter but no data\n", check_lba, lba);
			abort();
		}

		if(!test){
			ftl.read_error++;
			ftl.read--;
			return 0;
		}
		else 
			return 1;
	}
	else 
		return 0;
}

uint32_t block_read(block *target, uint32_t check_lba, uint32_t lba, bool isfirst){
	if(lba==655360){
		printf("break!\n");
	}
	for(uint32_t i=target->now_buffer_ptr; i>0; i--) {
		if(target->lba_buffer[i-1]==lba)
			return 1;
	}
	
	for(uint32_t i=target->now_page_ptr; i>0; i--){
		if(check_bitmap(target->ppa_valid_map, i-1)){
			if(page_read(&target->page_array[i-1], check_lba, lba, ((i-1)==0)&&isfirst)){
				return 1;
			}
		}
	}
	return 0;
}

uint32_t sp_read(sup_block *target, uint32_t check_lba, uint32_t
		lba){
	if(block_read(target->now, check_lba, lba, 0)){
		return 1;
	}
	
	std::deque<block*> *dataset=target->used_block_set;
	std::deque<block*>::reverse_iterator rit=dataset->rbegin();
	for(rit; rit!=dataset->rend() ; ++rit){
		if(block_read(*rit,check_lba, lba, (rit+1==dataset->rend())))
			return 1;
	}

	return 0;
}

uint32_t bff_read(uint32_t lba){
	uint32_t super_block_idx=(lba>>arg.lba_shift)%arg.NUM_SUP_BLOCK;
	uint32_t target_lba=lba;
	uint32_t lba_sets=arg.LPP;
	uint32_t boundary=lba/lba_sets*lba_sets;

	target_lba-=ftl.lba_length_map[lba];
	/*
	for(uint32_t i=lba+1; i-1>boundary; i--){
		if(check_bitmap(ftl.lba_valid_map, i-1)){
			break;
		}
		else{
			target_lba--;	
		}
	}*/

	if(target_lba < boundary){
		printf("error!\n");
		abort();	
	}

	return sp_read(&ftl.sb_array[super_block_idx], target_lba, lba);
}


uint32_t block_print(block *target){
	uint32_t filter_num=0;
	uint32_t page_invalid=0;
	for(uint32_t i=0; i<target->used; i++){
		printf("\t%u : ", i);
		if(check_bitmap(target->ppa_valid_map, i)){
			for(uint32_t j=0; j<arg.LPP; j++){
				printf("%u ", target->page_array[i].oob[j]);	
			}
			printf("(inv %u)", target->page_array[i].invalid_num);
			page_invalid+=target->page_array[i].invalid_num;
			printf("\n\t\t");
			for(uint32_t j=0; j<arg.LPP; j++){
				uint32_t res=target->page_array[i].filter_array[j]?1:0;
				printf("%u ", res);
				if(res) filter_num++;
			}
		}
		else{
			page_invalid+=arg.LPP;
			printf("(NULL)\n");
		}
		printf("\n");
	}
	printf("\n--------------->invalidation: %u filter_num:%u page_invalid:%u\n", target->invalid, filter_num, page_invalid);

	return filter_num;
}

uint32_t block_filter_num(block *target){
	uint32_t filter_num=0;
	for(uint32_t i=0; i<target->used; i++){
		if(check_bitmap(target->ppa_valid_map, i)){
			for(uint32_t j=0; j<arg.LPP; j++){
				uint32_t res=target->page_array[i].filter_array[j]?1:0;
				if(res) filter_num++;
			}
		}
	}
	return filter_num;
}

bool block_check(block *target){

	for(uint32_t i=0; i<target->used; i++){
		if(check_bitmap(target->ppa_valid_map, i)){
			p_page *page=&target->page_array[i];
			uint32_t filter_num=0, valid_num=4;
			for(uint32_t j=0; j<arg.LPP; j++){
				uint32_t res=page->filter_array[j]?1:0;
				if(res) filter_num++;
				if(res && !bf_check(page->filter_array[j], page->oob[j])){
					printf("block error!\n");
					block_print(target);
					printf("block error!\n");
					abort();
					return false;
				}
				if(check_bitmap(&page->invalid_bit, j)){
					valid_num--;
				}
			}
			if(valid_num != arg.LPP-page->invalid_num){
				printf("not match valid lba!\n");
				abort();
				return false;
			}
		}
	}
	return true;
}
