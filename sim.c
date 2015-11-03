#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <limits.h>


#define n_default 3 //number of individuals we're tracking
#define N_default 3 //assumed constant number of individuals at each site, simulation assumes: N <= n
#define M_default 10 //number of times to run the simulation
#define T_default 10 //Turns to simulate
#define m_default 0.1 //probability of migrating

#define mode_default 0 //output mode: 0 = 1 line per final state; 1 = 1 line per turn, plus locations (for plotting).
#define migration_mode_default 4 //migration mode: 0 = far; 2 = 1d, 4=2d-1sq-rook, 8=2d-1sq-rook+bishop




//parameters

int n = n_default; //number of individuals we're tracking
int N = N_default; //assumed constant number of individuals at each site, simulation assumes: N <= n
int M = M_default; //number of times to run the simulation
int T = T_default; //Turns to simulate
double m = m_default; //probability of migrating
int mode = mode_default; //Turns to simulate

int migration_mode = migration_mode_default;


struct site {
   bool settled; //indicates whether this site is occupied by a group
   long id[2];   //id (grid location) of the group occupying the site
   int nCurrentResidents; //current population of residents we care about   
   int nNewResidents; //number of residents who have just arrived   
   int *residents_current; //pointer to whichever resident set we're using
   int *residents_new;  //pointer to whichever resident set we're not using
  // int residents_one[n]; //vector of resident is at this location on odd turns
  // int residents_two[n]; //vector of resident is at this location on even turns
};



//simulation variables

struct site *sites; //start with as many sites as there are individuals : the most that could possibly be settled at one time
struct site *outposts; //temp array for new sites while migrants are moving;
int nOutposts = 0; //number of outposts
int *lineages;
int *parents;
long origin[2] = {0,0};
int Mprob; //integer constant for testing probability of migrating


//functions

void turn();
void usage();
void simulate();
void intialise();
void first_turn();
void print_results();
void print_header();
void print_locations();
void reset_sites();
void process_sites();
void print_collapsed_results();
void reset_site(struct site *);
void process_site(struct site *);
void upgrade_outposts();
void migrate(struct site *, int);
void settle(struct site *, long[]);
struct site * pick_new_site(long[]);
void coalesce_tree(int, int);
void coalesce_flat(int, int);
void (*coalesce)(int,int);
void parse_args(int, char *[]);


void pick_site_far(long[]);
void pick_site_two(long[]);
void pick_site_four(long[]);
void pick_site_eight(long[]);
void pick_site_twentyfour(long[]);
void (*pick_site)(long[]);



int main(int argc, char *argv[]){ 

	parse_args(argc, argv);
	
	srand(time(NULL)); //crank up the random number generator	
	Mprob = (int)(m*RAND_MAX); //integer that must be returned by rand() for real > m
	coalesce = &coalesce_flat; //print one lineage ID per person, not the lineage they merged into.
	
	switch(migration_mode){ //go n
		case 2: 
			pick_site = &pick_site_two;
			break;
		case 4: 
			pick_site = &pick_site_four;
			break;
		case 8: 
			pick_site = &pick_site_eight;
			break;
		case 24: 
			pick_site = &pick_site_twentyfour;
			break;			
		default: //go south
			pick_site = &pick_site_far;
			break;
	}
	
	
	
	
	parents = (int *) malloc(n*sizeof(int));
	lineages = (int *) malloc(n*sizeof(int));
	sites  = (struct site *) malloc(n*sizeof(struct site));
	outposts  =  (struct site *) malloc(n*sizeof(struct site));
	
	int i;
	
	//allocate memory
	for(i = 0; i < n; i++){
		sites[i].residents_current = (int *) malloc(n*sizeof(int));
		sites[i].residents_new = (int *) malloc(n*sizeof(int));
		outposts[i].residents_current = (int *) malloc(n*sizeof(int));
		outposts[i].residents_new = (int *) malloc(n*sizeof(int));
	}
	
	
	//simulate
	if(mode == 1){
		simulate();
	}else{
		for(i = 0; i <= M; i++){
			simulate();
		}
	}
	
	//free memory... not strictly necessary since we just exit anyway 
	for(i = 0; i < n; i++){
		free(sites[i].residents_current);
		free(sites[i].residents_new);
		free(outposts[i].residents_current);
		free(outposts[i].residents_new);
	}
	
	exit(0);
}

void parse_args(int argc, char *argv[]){ 

	if (argc == 1){usage(argv[0]);}
	int i;
	for (i = 1; i < argc; i++) { //parse command line arguments    
		if (argv[i][0] == '-') {
				 if (argv[i][1] == 'h') usage(argv[0]);
			else if (argv[i][1] == 'm') m = atof(argv[i+1]);
			else if (argv[i][1] == 'n') n = atoi(argv[i+1]);
			else if (argv[i][1] == 'N') N = atoi(argv[i+1]);
			else if (argv[i][1] == 'M') M = atoi(argv[i+1]);
			else if (argv[i][1] == 'T') T = atoi(argv[i+1]);
			else if (argv[i][1] == 'S') mode = atoi(argv[i+1]);
			else if (argv[i][1] == 'Z') migration_mode = atoi(argv[i+1]);
		}
	}
	
	if(n > N){
		N = n;
	}
	
	
}

void usage(char *name){
	fprintf (stderr, "\
	Usage: %s [OPTIONS]\n \
	Simulates a population of immortals who all start in one city, and then travel around on an infinite 2D grid, occassionally encountering each other, chopping off one another's heads and absorbing each other's power. Alternatively, simulates the coalescence of asexually reproducing lineages, with migration. \n\
	\n\
	Transition Rates \n\
	-m   probability of migrating each turn \n\
	\n\
	Constants \n\
	-n   Number of individuals to tracking (default: 100)\n\
	-N   Number of individuals at each site (N <= n; default: 100) \n\
	-T   Number of events (turns) to simulate (default: 1000) \n\
	-M   Number of times to re-run the simulation (in mode 0) (default: 1000) \n\
	\n\
	Simulation variants \n\
	-S   Sim. mode:\n\
	         0: Print only final lineages after T turns\n\
	         1: Print lineages after each turn to stdout, and locations to stderr (for visualisation, implies M=1). \n\
	-Z   Migration mode:\n\
			 0: default, far migration (rand(MAXINT))\n\
			 2:  2  directions, 1D\n\
			 4:  4  directions, 2D, rook-king\n\
			 8:  8  directions, 2D, king\n\
			 24: 24 directions, 2D, 2-sq king\n\
	\n\
	Example: %s -S 1 -n 1000 -N 1000 -T 1000000\n\
	Example: %s -S 0\n\
	", name, name, name);
	exit(0);
}

void simulate(){

	intialise();
	
	int t;
	if(mode == 1){
		print_results();
		print_locations();
		for(t = 0; t < T; t++){
			//fprintf(stderr, "----\n");
			//print_locations();
			turn();
			print_results();
			print_locations();
		}
	}else{
		for(t = 0; t < T; t++){
			turn();			
		}
		print_results();
		//print_locations();
	}
} 

void print_locations(){
	int i; int j; int locations[n][2];
	
	for(i = 0; i < n; i++){ 
		locations[i][0] = INT_MAX; 
		locations[i][1] = INT_MAX;
	}
	
	int guy;
	for(i = 0; i < n; i++){
		if(sites[i].settled) {
			for(j = 0; j <  sites[i].nCurrentResidents; j++){
				guy = *(sites[i].residents_current+j);
				locations[guy][0] = sites[i].id[0];
				locations[guy][1] = sites[i].id[1];
			}
		}
	}
	
	if(locations[0][0] == INT_MAX){
		fprintf(stderr, "      ");
	} else {
		fprintf(stderr, "%02d,%02d", locations[0][0], locations[0][1]);
	}
	
	for(i = 1; i < n; i++){ 
		if(locations[i][0] == INT_MAX)
			fprintf(stderr, ";      ");
		else 
			fprintf(stderr, "; %02d,%02d", locations[i][0], locations[i][1]);
	}
	fprintf(stderr, "\n");
}

void print_header(){
	int i;
	printf("%d", 0);	
	for(i = 1; i < n; i++){ //iterate through individuals			
		printf(", %d", i);
		fprintf(stderr, ";    %d", i);
	}
	printf("\n");
	
	if(mode == 1){
		fprintf(stderr, "   %d", 0);
		for(i = 1; i < n; i++){ //iterate through individuals					
			fprintf(stderr, ";    %d", i);
		}
		fprintf(stderr, "\n");
	}
	
}

void print_collapsed_results(){
	int i;
	int lineages2[n];
	for(i = 0; i < n; i++){ //iterate through individuals		
		lineages2[i] = lineages[i];
	}
	bool done = false;
	while (!done){
	done = false;
		for(i = 0; i < n; i++){ //iterate through individuals		
			if(lineages2[i] != -1)
			 if(lineages2[lineages2[i]] != -1) lineages2[i] = lineages2[lineages2[i]];
			done = true;
		}
	}
	printf("%d",  lineages2[0]);
	for(i = 1; i < n; i++){ //iterate through individuals		
		printf(", %d", lineages2[i]);
	}
}

void print_results(){
	int i;
	printf("%d",  lineages[0]);
	for(i = 1; i < n; i++){ //iterate through individuals		
		printf(", %d", lineages[i]);
	}
	printf("\n");
}

void intialise(){
	int i;
	for(i = 0; i < n; i++){
		sites[i].settled = 0;
		outposts[i].settled = 0;
		lineages[i] = i;
	}

	settle(&(sites[0]), origin); //establish the origin
	sites[0].nCurrentResidents = n;//inhabit the origin
	for(i = 0; i < n; i++) sites[0].residents_current[i] = i;//inhabit the origin	
	
	first_turn();
}

void first_turn(){ //only migration, no coalescence
	int i;
	for(i = 0; i < n; i++){ //everyone gets a chance to flee before the big battle	
		migrate(&(sites[0]), i); //migrate
	}	
	reset_sites();
	upgrade_outposts();
}

void turn(){
	
	process_sites();
	
	reset_sites();//once we've decided where everyone's going, we set up the sites for next round
	
	upgrade_outposts(); //empty sites should now be vacant, so we transform outposts into settlements. This is inefficient, but should happen rarely, unless the population becomes far-flung without anyone coalescing. Doing it this way saves us a bunch of looping in the typical cases.
	
}

void upgrade_outposts(){
	int i; 
	int j;
	bool siteFound;
	for(i = 0; i < nOutposts; i++){ //for each outpost
		siteFound = false;
		for(j = 0; j < n; j++){ //for each potential site
			if(!sites[j].settled){
				 settle(&(sites[j]), outposts[i].id);
				 sites[j].nNewResidents = outposts[i].nNewResidents;
				 *(sites[j].residents_new) = *(outposts[i].residents_new);
				 reset_site(&(sites[j]));
				 siteFound = true;
				 outposts[i].settled = 0;
				 break;
			 }
		 }
		 if(!siteFound) { //we've gone through the loop and not found a settlement site. Bail.
			 fprintf(stderr, "Could not find a vacant site to settle. This shouldn't happen. Must be a bug.");
			 exit(1); 
		 }
	}
	nOutposts = 0; //reset the outposts
}

void process_sites(){
	int i; 
	for(i = 0; i < n; i++){ //iterate through sites
		if(sites[i].settled) { //only process settled sites
			process_site(&(sites[i]));
		}
	}
}

void process_site(struct site * here){
	int j;
	int k;
	for(j = 0; j < here->nCurrentResidents; j++){ //iterate through individuals at the site
		parents[j] = rand() % N;	
		for(k = 0; k< j; k++){ //iterate through individuals who've already got parents
			if(parents[j] == parents[k]){//same parent
				//merge this lineage into the other one
				(*coalesce)(*(here->residents_current+k), *(here->residents_current+j));
				parents[j] = -1; //set their parent as merged
			}
		}
		if (parents[j] != -1) { //if we didn't coalesce
			migrate(here, *(here->residents_current+j) ); //migrate
		}
	}
}

void reset_sites(){
	int i; 
	for(i = 0; i < n; i++){ //iterate through sites
		if(sites[i].settled) { //only process settled sites
			reset_site(&(sites[i]));//reset sites
		}
	}
}

void reset_site(struct site * here){ 
	if(here->nNewResidents == 0){ //vacate empty sites, so there's always enough space for new settlers
		here->settled = 0;
	}else{
		int *temp; 
		here->nCurrentResidents = here->nNewResidents;
		here->nNewResidents = 0;

		temp = here->residents_current;
		here->residents_current = here->residents_new;
		here->residents_new = temp;
	}	
}

void migrate(struct site * here, int guy){
	if(rand() > Mprob){ //we stayed here		
		here->residents_new[here->nNewResidents++] = guy;
	} else {
		struct site *there =  pick_new_site(here->id);
		there->residents_new[there->nNewResidents++] = guy;		
	}
}

struct site * pick_new_site(long here_id[2]){ //sets the pointer "there" to point to a randomly selected site connected to this one..
	
	long there_id[2];
	there_id[0] = here_id[0];
	there_id[1] = here_id[1];
	pick_site(there_id);
	
	//find it in the list
	int i;
	int k = -1; //use k to store the location of an empty site, just in case we don't find out target
	for(i = 0; i < n; i++){ //iterate through sites
		if(sites[i].settled) { //only process settled sites
			if(sites[i].id[0] == there_id[0] && sites[i].id[1] ==there_id[1]) 
				return  &(sites[i]); //we've found our site
		} else if (k == -1){ //checking this slows things down, but ensures lower-indexed sites are settled first, making searches quicker
			k = i;	
		}
	}
	//we went through the list without finding our target site, we need to settle a new one
	if(k != -1){ //we found a vacant site, settle it
			settle(&(sites[k]), there_id);
			return &(sites[k]);
		} else {//we didn't find a vacant site either, create an outpost
			settle(&(outposts[nOutposts]), there_id);
			return &(outposts[nOutposts++]);
		}
}

void settle(struct site * here, long id[2]){ //creates an empty settlement	
	here->settled = 1;
	here->id[0] = id[0];   
	here->id[1] = id[1];   
	here->nCurrentResidents = 0;
	here->nNewResidents =0;
	//here->residents_current = &(here->residents_one[0]);
	//here->residents_new = &(here->residents_two[0]); 
}

void coalesce_tree(int winner, int loser){
	lineages[loser] = winner;
}

void coalesce_flat(int winner, int loser){
	int i;	
	for(i = 0; i < n; i++){ //iterate through individuals		
		if(lineages[i] == loser) lineages[i] = winner;
	}
}


void pick_site_far(long id[2]){
	id[0] = rand();
	id[1] = rand();
}
void pick_site_two(long id[2]){
	switch(rand() % 2){ //go n
		case 0: //go north
			id[0]+=1;
			break;
		case 1: //go south
			id[0]-=1;		
	}		
}
void pick_site_four(long id[2]){
	switch(rand() % 4){ //go n
		case 0: //go north
			id[0]+=1;
			break;
		case 1: //go south
			id[0]-=1;
			break;
		case 2: //go east
			id[1]+=1;
			break;
		case 3: //go west
			id[1]-=1;		
			break;							
	}		
}
void pick_site_eight(long id[2]){
	switch(rand() % 8){ //go n
		case 0: //go north
			id[0]+=1;
			break;
		case 1: //go south
			id[0]-=1;
			break;
		case 2: //go east
			id[1]+=1;
			break;
		case 3: //go west
			id[1]-=1;		
			break;							
		case 4: //go north-east
			id[0]+=1;
			id[1]+=1;		
			break;							
		case 5: //go north-west
			id[0]+=1;
			id[1]-=1;		
			break;							
		case 6: //go south-east
			id[0]-=1;
			id[1]+=1;		
			break;							
		case 7: //go south-west
			id[0]-=1;
			id[1]-=1;		
			break;							
	}
}
void pick_site_twentyfour(long id[2]){
		
	int dx = 0;
	int dy = 0;
	
	while ((dx == dy) && (dx == 0) ) 
	{
		dx = (rand() % 5)-2;
		dy = (rand() % 5)-2;
	}
	id[0]-=dx;
	id[1]+=dy;
}	

