#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "TCB.h"
#define MAX_TCB 5
#define THREAD_STACK_SIZE 1024
//全局变量
TCB * gTCB[MAX_TCB] = {NULL};
TCB * gpCur = NULL;
//函数声明
int ThreadCreate(void (*func),void * arg);
void Run();
void Yield();
void ThreadExit(); 
void PrintMemory(long * x);//查看内存，调试用 
int ThreadCreate(void (*func),void * arg){
	int i,tcb_id;
	for(i = 0;i < MAX_TCB;i++){
        if(gTCB[i] == NULL)
        	break;
	}//gTCB查空指针 
	if(i == MAX_TCB){
		tcb_id = -1;
		return tcb_id;
	}//无空指针
	tcb_id = i;//有空指针
	TCB * newTCB = (TCB*)malloc(sizeof(TCB));
	memset(newTCB,0,sizeof(TCB));
	newTCB->t_id = tcb_id;
	newTCB->state = 1;
	newTCB->pStack = (long *)malloc(THREAD_STACK_SIZE);
	memset(newTCB->pStack,0,THREAD_STACK_SIZE);//初始化一个新的TCB
	long * stack = newTCB->pStack + THREAD_STACK_SIZE / sizeof(long);//stack指向栈最底
	stack--;
	*stack = (long)arg;//arg入栈 
	stack--;
	*stack = (long)ThreadExit;//exit入栈 
	stack--;
	*stack = (long)func;//func入栈
	stack--;
	*stack = (long)(stack + 2);//
	newTCB->ebp = (long)stack;
	stack--;
	*stack = 0;//eax
	stack--;
	*stack = 0;//ebx
	stack--;
	*stack = 0;//ecx
	stack--;
	*stack = 0;//edx
	stack--;
	*stack = 0;//esi
	stack--;
	*stack = 0;//edi	
	newTCB->esp = (long)stack;
	gTCB[tcb_id] = newTCB;//更新gTCB
	//PrintMemory(newTCB->pStack + THREAD_STACK_SIZE/sizeof(long));
	return tcb_id;
}
void PrintMemory(long * x){
	int i = 0;
	for(i = 0;i < 10;i++){
		x--;
		printf("addr:%p\t | data:%p\n",x,*(x));
	}
}
void ThreadExit(){
	int i = gpCur->t_id;
	free(gTCB[i]->pStack);
	free(gTCB[i]);
	gTCB[i] = NULL;
	printf("\n线程%d退出\n",i);
	Run();
}
void Yield(){
	TCB * pCur;
	TCB * pNext;
	pCur = gpCur;
	int i = pCur->t_id;
	while(1){
		i = (i + 1)%MAX_TCB;
		if(gTCB[i] != NULL)
		    break;
	}
	if(i == pCur->t_id)
		return;
	pNext = gTCB[i];
	gpCur = pNext;
    __asm__ __volatile__
    (
            "pushl %%edi \n"
            "pushl %%esi \n"
            "pushl %%edx \n"
            "pushl %%ecx \n"
            "pushl %%ebx \n"
            "pushl %%eax \n"
            "movl %%esp, %0 \n" 
            "movl %%ebp, %1 \n"
            : "=m" (pCur->esp),
			  "=m" (pCur->ebp) 
            :
            : "memory"
    );//保存pCur的寄存器
    __asm__ __volatile__
    (
            "movl %0, %%esp \n" 
            "movl %1, %%ebp \n"
            "popl %%edi \n"
            "popl %%esi \n"
            "popl %%edx \n"
            "popl %%ecx \n"
            "popl %%ebx \n"
            "popl %%eax \n"
            : 
            : "m" (pNext->esp),
              "m" (pNext->ebp)
            :"esp","ebp"
    );//修改esp与ebp至pNext 
}
void Run(){
	int i = 0;
	for(i = 0;i < MAX_TCB;i++){
		if(gTCB[i] != NULL)
		    break;
	}//遍历gTCB
	if(i == MAX_TCB)
		exit(0);//如果没有线程
	gpCur = gTCB[i];
	TCB* pCur = gpCur;//临时变量
    __asm__ __volatile__
    (
            "movl %0, %%esp \n" 
            "movl %1, %%ebp \n"
            "popl %%edi \n"
            "popl %%esi \n"
            "popl %%edx \n"
            "popl %%ecx \n"
            "popl %%ebx \n"
            "popl %%eax \n"
            : 
            : "m" (pCur->esp),
              "m" (pCur->ebp)
            :"esp","ebp"
    );//__asm__(汇编语句模板: 输出部分: 输入部分: 破坏描述部分)*/
	
}
void A(void * args){
	int A = (long)args;
	while(A-- > 0){
		printf("A");
		Yield();
	}
}
void B(void * args){
	int A = (long)args;
	while(A-- > 0){
		printf("B");
		Yield();
	}
}
int main(int argc, char *argv[]) {
	void * arg1 = (void *)(5000);
	void * arg2 = (void *)(3000);
	int t1 = ThreadCreate(A,arg1);
	printf("线程创建成功:t_id = %d\n",t1); 
	int t2 = ThreadCreate(B,arg2);
	printf("线程创建成功:t_id = %d\n",t2); 
	Run();
	return 0;
}

