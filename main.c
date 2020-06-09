#include "bloom_calc.h"
#include "device.h"
#include <stdio.h>
#include <getopt.h>
#include <string.h>

extern bloom_arg arg;
extern bloom_ftl ftl;

uint32_t total_lpa;
uint32_t input_OP;
float ratio_lpa;
char __type;
enum{
	RANDOMBENCH, SEQBENCH
};

int bench_set_params(int argc, char **argv, char **temp_argv);

char* get_bench_type(char input_type){
	switch(input_type){
		case RANDOMBENCH:
			return (char*)"RAND";
			break;
		case SEQBENCH:
			return (char*)"SEQ";
			break;
	}
	return NULL;
}

int main(int argc, char*argv[]){

	uint64_t write_req=0;
	uint64_t read_req=0;

	char *temp_argv[20];
	int temp_cnt=bench_set_params(argc, argv, (char **)&temp_argv);
	bff_init(temp_cnt, temp_argv);

	uint64_t total_lpa=arg.TOTAL_DEV_SIZE/LBASIZE;
	uint64_t range=(total_lpa*100/(100+input_OP));

	total_lpa=range/ratio_lpa;

	printf("\ttotal request %lu (ratio:%.2f%% %f)\n", total_lpa, ((float)100/ratio_lpa), ratio_lpa);
	printf("\trange %lu\n", range);
	printf("\trequest type : %s\n", get_bench_type(__type));
	printf("\tOP : %u\n", input_OP);
	printf("\tbloomfilter per superblock: %lu\n", arg.LPP * arg.BPS * arg.PPB);
	printf("\tfpr: %lf\n", arg.bloom_fpr);


	for(uint64_t i=0; i<range; i++){
		uint32_t lpa;
		lpa=i%range;
		if(i%102400==0){
			printf("fillseq %lu/%lu\n", i, range);
		}
		write_req++;
		bff_write(lpa);	
	}


	srand(0);
	for(uint64_t i=0; i<total_lpa; i++){
		uint32_t lpa;
		switch(__type){
			case RANDOMBENCH:
				lpa=rand()%range;
				break;
			case SEQBENCH:
				lpa=i%range;
				break;
		}

		if(i%102400==0){
			printf("write %lu/%lu\n", i, total_lpa);
		}
		write_req++;
		bff_write(lpa);
	}

	uint32_t error=0;
	//srand(0);
	
	for(uint64_t i=0; i<range/10; i++){

		uint32_t lpa;
		switch(__type){
			case RANDOMBENCH:
				lpa=rand()%range;
				break;
			case SEQBENCH:
				lpa=i%range;
				break;
		}
		if(i%102400==0){
			printf("read %lu/%lu\n", i, total_lpa/10);
		}
		if(!bff_read(lpa)){
			error++;
			printf(" %lu lpa :%u error!!!\n", i, lpa);
			abort();
		}
		read_req++;
	}

	printf("total request %lu\n", total_lpa);
	printf("\t WAF %.3lf\n", (double)((ftl.gc_write+ftl.rb_write+ftl.write))*arg.LPP/write_req);
	printf("\t Flash Write %.3lf\n", (double)((ftl.gc_write+ftl.rb_write+ftl.write))/write_req);
	printf("\t RAF %.3lf\n", (double)(ftl.read+ftl.read_error)*arg.LPP/read_req);
	printf("\t Flash Read %.3lf\n", (double)(ftl.read+ftl.read_error)/read_req);
	bff_free();	
	printf("error:%u\n", error);

}


int bench_set_params(int argc, char **argv, char **temp_argv){
	struct option options[]={
		{"op", 1, 0, 0},
		{"bench", 1, 0, 0},
		{"ratio", 1, 0, 0,},
		{0,0,0,0}
	};

	int temp_cnt=0;
	for(int i=0; i<argc; i++){
		if(strncmp(argv[i],"--op",strlen("--op"))==0) continue;
		if(strncmp(argv[i],"--bench",strlen("--bench"))==0) continue;
		if(strncmp(argv[i],"--ratio",strlen("--ratio"))==0) continue;
		temp_argv[temp_cnt++]=argv[i];
	}

	int index;
	int opt; 

	bool opset=false;
	bool benchset=false;
	bool ratioset=false;

	opterr=0;
	while((opt=getopt_long(argc, argv,"",options, &index))!=-1){
		if(opt!=0) continue;
		switch(index){
			case 0:
				if(optarg!=NULL){
					input_OP=atoi(optarg);
					opset=true;
				}
				break;
			case 1:
				if(optarg!=NULL){
					if(optarg[0]=='r'){
						__type=RANDOMBENCH;
					}
					else if(optarg[0]=='s'){
						__type=SEQBENCH;
					}
					else{
						printf("no bench type!!\n");
						abort();
					}
					benchset=true;
				}
				break;
			case 2:
				if(optarg!=NULL){
					ratio_lpa=(float)atof(optarg);
					ratioset=true;
				}
				break;
		}
	}

	if(!opset) input_OP=OP;
	if(!benchset) __type=RANDOMBENCH;
	if(!ratioset) ratio_lpa=2;

	optind=0;
	return temp_cnt;
}
