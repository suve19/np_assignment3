#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>


/* Std start to main, argc holds the number of arguments provided to the executable, and *argv[] an 
   array of strings/chars with the arguments (as strings). 
*/
int main(int argc, char *argv[]){

//reti = regcomp(&regex, "[[:digit:]]{1,10}\\.[[:digit:]]{6}",REG_EXTENDED);
//	char *expression="[[A-Za-z0-9_]]+";
	char *expression="[A-Za-z_]+";
	regex_t regularexpression;
	int reti;
	
	reti=regcomp(&regularexpression, expression,REG_EXTENDED);
	if(reti){
		fprintf(stderr, "Could not compile regex.\n");
		exit(1);
	}
	
	for(int i=0;i<argc;i++){
		reti=regexec(&regularexpression, argv[i],1,NULL,0);
		if(!reti){
			printf("Match, %s matched expression %s \n",argv[i], expression);
		} else {
			printf("No Match found, %s != %s \n",argv[i], expression);
		}
	}
	
	regfree(&regularexpression);
	return(0);

}
