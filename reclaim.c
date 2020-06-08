#include "bloom_calc.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include<stdio.h>

extern bloom_ftl ftl;
extern bloom_arg arg;
 
bool debug;

typedef struct pair{
	uint32_t lba; //4KB
	uint32_t p_piece; 
	uint32_t time; //4KB
	bool isbuffer;
}pair;

std::vector<uint32_t> *merging_page(sup_block *target,
		pair *lba_target_array, uint32_t pair_idx, bool isgc, uint32_t *filter_res);
/*
std::vector<uint32_t> **merging_page(sup_block *target,
		pair *lba_target_array, uint32_t pair_idx, bool isgc, uint32_t *filter_res);

uint32_t get_lcm(uint32_t a, uint32_t b){
	uint32_t small, big;
	if(a>=b){
		small=b;
		big=a;
	}else{
		big=b;
		small=a;
	}

	uint32_t mok, nmg;
	while(1){
		mok=big/small;
		nmg=big-mok*small;
		if(nmg==0){
			uint32_t gcm=small;
			return a*b/gcm;
		}
	}
	return 0;
}

uint32_t get_fit_filter_cnt(uint32_t lpp, uint32_t target, uint32_t target_sets){
	uint32_t res=0;
	if(lpp%target==0){
		//align;
		res=target_sets/lpp * target;
	}
	else{
		uint32_t res=0;
		uint32_t lcm=get_lcm(lpp, target);

	}
	res=(target_sets%lpp?1:0);
	return res;
}

uint32_t filter_estimation(std::vector<uint32_t> ** arr){
	uint32_t cnt=0;
	cnt+=arr[arg.LPP-1]->size()/arg.LPP;
	uint32_t a=0;
	uint32_t b=arg.LPP-1-i;
	while(a!=b){
		uint32_t as=arr[a]->size()/(a+1);
		uint32_t bs=arr[b]->size()/(b+1);
		if(as<bs){
			cnt+=as;
			bs-=as;

		}else if(as>bs){
			cnt+=bs;
			as-=bs;
		}else{
			cnt+=as;	
		}
		a++;
		b--;
	}
	cnt+=arr[a]->size()/arg.LPP;
	return cnt;
}
*/
void remove_deque(std::deque<block *> *dq, block *t){
	std::deque<block *>::iterator it=dq->begin();
	bool isfound=false;
	int i=0; 
	for(it; it!=dq->end(); ++it){
		if(*it==t) {
			isfound=true;	
			break;
		}
		i++;
	}

	if(!isfound){
		printf("no way!! in remove deque");
		abort();
	}

	dq->erase(dq->begin()+i);
}

/*
 force param is for GC
 */
void trim_blocks(sup_block *target, bool force){
	uint32_t idx=0;
	block **trim_set=(block**)malloc(sizeof(block*) * arg.BPS);
	std::deque<block*>  *dataset=target->used_block_set;
	std::deque<block*>::iterator it=dataset->begin();
	for(it; it!=dataset->end() ; ++it){
		block *tblock=*it;
		if(tblock->invalid==arg.PPB || force ){
			trim_set[idx++]=tblock;
		}
	}
	
	for(uint32_t i=0; i<idx; i++){
		uint32_t block_idx=trim_set[i]->idx;
		uint32_t used=trim_set[i]->used;
		block_free(trim_set[i]);
		ftl.trim++;
		remove_deque(target->used_block_set, trim_set[i]);
		block_init(trim_set[i], block_idx);
		target->free_block_set->push(trim_set[i]);
		target->used-=used;
	}
	free(trim_set);

	if(target->now && target->now->invalid==arg.PPB){
		uint32_t block_idx=target->now->idx;
		uint32_t used=target->now->used;
		block_free(target->now);
		ftl.trim++;
		block_init(target->now, block_idx);
		target->free_block_set->push(target->now);
		target->now=NULL;
		target->used-=used;
	}

	if(!target->now && target->free_block_set->size()){
		target->now=target->free_block_set->front();
		target->free_block_set->pop();
	}
}

void invalid_piece(sup_block *target, pair *p){
	if(p->isbuffer){
		block *tblock=target->now;
		for(uint32_t i=p->p_piece%arg.LPP; i<arg.LPP-1; i++){
			tblock->lba_buffer[i]=tblock->lba_buffer[i+1];
		}
		tblock->now_buffer_ptr--;
	}
	else{

		uint32_t block_idx=p->p_piece/arg.LPP/arg.PPB; 
		uint32_t page_idx=p->p_piece/arg.LPP % arg.PPB;
		block *tblock=&ftl.block_super_set[block_idx];

		p_page *page=&tblock->page_array[page_idx];
		uint32_t inter_idx=p->p_piece%arg.LPP;

		if(page->oob[inter_idx]!=p->lba){
			printf("not matched!\n");
			abort();
		}

		if(set_bitmap(&page->invalid_bit, inter_idx)){
			page->invalid_num++;
		}

		if(page->filter_array[inter_idx]){
			bf_free(page->filter_array[inter_idx]);
			page->filter_array[inter_idx]=NULL;
			target->filter_num--;
		}

		if(page->invalid_num == arg.LPP){
			if(check_bitmap(tblock->ppa_valid_map, page_idx)){
				tblock->invalid++;
				unset_bitmap(tblock->ppa_valid_map, page_idx);
			}
		}
		if(tblock->invalid > arg.PPB){
			printf("over invalidate!\n");
			abort();
		}
	}
}

int cmppair(const void *_a, const void *_b){
	pair *a=(pair*)_a; 
	pair *b=(pair*)_b;

	int res=a->lba - b->lba;
	if(res==0){
		return b->time - a->time;
	}
	return res;
}

uint32_t __rebloom(sup_block *target){
	uint32_t max_lba=arg.BPS * arg.PPB * arg.LPP;
	uint32_t pair_idx=0;
	pair *lba_target_array=(pair*)malloc(sizeof(pair) * max_lba);

	/*used block*/
	std::deque<block*>  *dataset=target->used_block_set;
	std::deque<block*>::iterator it=dataset->begin();
	block *print_block=NULL;
	for(it; it!=dataset->end() ; ++it){
		block *tblock=*it;
		for(uint32_t i=0; i<arg.PPB; i++){
			p_page *page=&tblock->page_array[i];
			if(!check_bitmap(tblock->ppa_valid_map, i)) continue;
			ftl.rb_read++;
			for(uint32_t j=0; j<arg.LPP; j++){
				lba_target_array[pair_idx].lba=page->oob[j];
				lba_target_array[pair_idx].time=pair_idx;
				lba_target_array[pair_idx].p_piece=(tblock->idx * arg.PPB + i) * arg.LPP + j;
				lba_target_array[pair_idx].isbuffer=false;
				pair_idx++;
			}
		}
		//block_print(tblock);
		print_block=tblock;
	}

	/*current block*/
	block *tblock=target->now;

	if(tblock){
		print_block=tblock;
		for(uint32_t i=0; i<tblock->used; i++){
			p_page *page=&tblock->page_array[i];
			if(!check_bitmap(tblock->ppa_valid_map, i)) continue;
			for(uint32_t j=0; j<arg.LPP; j++){
				lba_target_array[pair_idx].lba=page->oob[j];
				lba_target_array[pair_idx].time=pair_idx;
				lba_target_array[pair_idx].p_piece=(tblock->idx * arg.PPB + i) * arg.LPP + j;
				lba_target_array[pair_idx].isbuffer=false;
				pair_idx++;
			}	
		}

		/*current page buffer*/
		if(tblock->now_buffer_ptr!=0){
			printf("not allowed!\n");
			abort();
		}
	}
	if(debug){
		printf("break!\n");
	}

	uint32_t filter_estimate=0;
	std::vector<uint32_t> *new_write_lba=merging_page(target, lba_target_array, pair_idx, false, &filter_estimate);
	
	/*trim block*/
	trim_blocks(target, false);

	if(debug){
		printf("break!\n");
	}

	if(target->filter_num+filter_estimate > (arg.PPB * arg.BPS * arg.LPP /2) + arg.filter_additional_space){
		block_print(print_block);
		printf("over filter break!!!\n");
		abort();
	}

retry:
	if(target->used + new_write_lba->size()/arg.LPP + 
			(new_write_lba->size()%arg.LPP?1:0) > arg.PPB * arg.BPS){
		if(arg.BPS==1){
			target->used_block_set->push_back(target->now);
			target->now=NULL;
		}
		gc(target);
		goto retry;
	}
	
	for(uint32_t i=0;i<new_write_lba->size(); i++){
		sp_write(target, (*new_write_lba)[i], REBLOOM);
	}
	free(lba_target_array);
	return 1; 
}

uint32_t rebloom(sup_block *target){
	//printf("rebloom!!!!\n");
	static int cnt=0;
	if(cnt++){
		if(cnt==624){
		//	printf("break!\n");
		//	debug=true;
		}
	//	printf("cnt:%d\n", cnt);
	}
	return __rebloom(target);
}


uint32_t __gc(sup_block *target){
	if(debug){
		printf("break!\n");
	}
	uint32_t max_lba=arg.PPB * arg.LPP;
	uint32_t pair_idx=0;
	pair *lba_target_array=(pair*)malloc(sizeof(pair) * max_lba);

	/*used block*/
	std::deque<block*>  *dataset=target->used_block_set;
	std::deque<block*>::iterator it=dataset->begin();
	block *print_block=NULL;
	uint32_t temp_idx=0;
	for(it; it!=dataset->end() ; ++it){
		block *tblock=*it;
		if(temp_idx==0){
			print_block=tblock;
			temp_idx++;
			continue;
		}
		if(print_block->invalid < tblock->invalid){
			print_block=tblock;
		}
	}

	block *tblock=print_block;
	for(uint32_t i=0; i<arg.PPB; i++){
		p_page *page=&tblock->page_array[i];
		if(!check_bitmap(tblock->ppa_valid_map, i)) continue;
		ftl.gc_read++;
		for(uint32_t j=0; j<arg.LPP; j++){
			if(check_bitmap(&page->invalid_bit,j)) continue;
			lba_target_array[pair_idx].lba=page->oob[j];
			lba_target_array[pair_idx].time=pair_idx;
			lba_target_array[pair_idx].p_piece=(tblock->idx * arg.PPB + i) * arg.LPP + j;
			lba_target_array[pair_idx].isbuffer=false;
			pair_idx++;
		}
	}
	
	uint32_t filter_estimate=0;
	if(debug)
		block_print(print_block);

	std::vector<uint32_t> *new_write_lba=merging_page(target,lba_target_array, pair_idx, true, &filter_estimate);
	
	trim_blocks(target, (true && (arg.BPS==1)));
	
	if(target->filter_num+filter_estimate > (arg.PPB * arg.BPS * arg.LPP /2) + arg.filter_additional_space){
		block_print(print_block);
		printf("over filter break!!!\n");
		abort();
	}
	
	if(target->used + new_write_lba->size()/arg.LPP + 
			(new_write_lba->size()%arg.LPP?1:0) > arg.PPB * arg.BPS){
		printf("it can't be\n");
		abort();
	}
	
	for(uint32_t i=0;i<new_write_lba->size(); i++){
		sp_write(target, (*new_write_lba)[i], GC);
	}
	free(lba_target_array);
	if(debug)
		block_print(print_block);
	return 1;
}

uint32_t gc(sup_block *target){
	static int cnt=0;
	if(cnt++){
		if(cnt==1){
			debug=true;
		}
	}
	//printf(" %u GC!!!\n", cnt);
	return __gc(target);
}


std::vector<uint32_t> *merging_page(sup_block *target, 
		pair *lba_target_array, uint32_t pair_idx, bool isgc, uint32_t *filter_res){
	qsort(lba_target_array, pair_idx, sizeof(pair), cmppair);

	/*std::vector<uint32_t> **new_write_lba_res=(std::vector<uint32_t>**)malloc(sizeof(std::vector<uint32_t>*) *arg.LPP);
	for(uint32_t i=0; i<arg.LPP; i++){
		new_write_lba_res[i]=new std::vector<uint32_t>();
	}*/

	std::vector<uint32_t> *new_write_lba=new std::vector<uint32_t> ();

	uint32_t i=0;
	pair **buffer=(pair **)malloc(sizeof(pair *) * arg.LPP);

	uint32_t filter_estimate=0;
	while(i<pair_idx){
		pair *p=&lba_target_array[i];
		buffer[0]=p;

		uint32_t pre_lba=p->lba;
		
		uint32_t max_merge=arg.LPP - pre_lba %arg.LPP;	
		uint32_t cnt=0;
		uint32_t j=1;
		while ( cnt < max_merge && (i+j)<pair_idx){
			if(i+j>=pair_idx){
				printf("break!\n");
			}
			pair *p2=&lba_target_array[i+j];
			if(pre_lba==p2->lba){
				//invalidate same lba
				invalid_piece(target,p2);
				j++;
				continue;
			}
			else if(pre_lba+1 == p2->lba){
				j++;
				cnt++;
				buffer[cnt]=p2;
				pre_lba=p2->lba;
			}
			else{
				//no relation
				break;
			}
		}

		bool new_write_check=false;
		if(cnt==0){
			//recent data without relationship	
			 if(isgc){
				//if gc logic, it needs rewrite;
				new_write_check=true;	
			}
		}
		else{
			if(isgc) new_write_check=true;
			else{
				for(uint32_t k=1; k<=cnt; k++){
					//if same page, they do not need to rewrite.
					if(p->p_piece/4 != buffer[k]->p_piece/4){
						new_write_check=true;
						break;
					}
				}
			}
		}

		if(new_write_check){
			filter_estimate++;
			for(uint32_t k=0; k<=cnt; k++){
				invalid_piece(target,buffer[k]);
				new_write_lba->push_back(buffer[k]->lba);
			}
		}
		i+=j;	
	}
	free(buffer);
	if(filter_res){
		*filter_res=filter_estimate;
	}
	return new_write_lba;
}
