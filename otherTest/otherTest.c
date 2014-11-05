#include<stdio.h>
int main(){
	
	int *p = NULL,*q = NULL;
	p = (int*)malloc(sizeof(int));
	q = p;
	free(q);//freed
	q = NULL;
	
	p = (int*)malloc(sizeof(int));// not freed
	
	return 0;
}

