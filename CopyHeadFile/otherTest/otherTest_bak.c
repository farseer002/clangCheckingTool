#include<stdlib.h>
#include<stdio.h>
#include<math.h>
#include"another.h"
#include "1.h"
#include "nextlayer/nl.h"
int* fact(){
	int *p = NULL;
	return p;
}
void what(int a){
    int b;
}
void func_2(){
    int *a;
    a = (int*)malloc(sizeof(int));
}
int main(){
	int a;
	double b;
	a = 0;
	b = 2.0;
	int c = b/a;
	int q[32][2];
	q[-1][2] = 3;
	int *p;
	
	p = (int *)malloc(sizeof(int));
	*p = 3;
	//printf("w *p = %d,%x\n",*p,p);
	{
	    int *p;
	    p = (int *)malloc(sizeof(int));
	    //printf("*p = %d,%x\n",*p,p);
	    free(p);
	}

    //free(p);
/*
	char *q;
	q = (char*)malloc(sizeof(char));
	char *qq;
	qq = q;
	if(qq){
		free(qq);
	}
	*/
	int *pp = p;
	
	p = (int*)malloc(sizeof(int));

	free(p);
	free(pp);
	func_2();
	func_2();
	
	double h = -1;
	h = sqrt(h);
	
/*	p = (int*)fact();
	p = (int*)malloc(sizeof(int));
	free(p);
	func_2();
	h2();
	what(a);

	
	int **p2;
	p2 = (int **)malloc(sizeof(int*));
	*p2 = (int *)malloc(sizeof(int));
	printf("%x\n",*p2);
	**p2 = 5;
	printf("%x\n",*p2);
	free(*p2);

	free(p2);
	
	
	int ***p3;
	p3 = (int ***)malloc(sizeof(int**));
	*p3 = (int **)malloc(sizeof(int*));
	**p3 = (int *)malloc(sizeof(int));
	free(**p3);
	//free(*p3);
	free(p3);
*/	
	return 0;
}

