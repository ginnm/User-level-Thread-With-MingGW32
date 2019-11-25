# User-level-Thread-With-MingGW32
实现了用户级多线程
- [User-level-Thread-With-MingGW32](#user-level-thread-with-minggw32)
- [1 用户级线程的概念](#1---------)
- [2 用户级线程的设计和实现](#2------------)
  * [2.1 用户级线程切换Yield()](#21--------yield--)
    + [2.1.1 为什么先设计Yield()而不是ThreadCreate()](#211-------yield-----threadcreate--)
    + [2.1.2 Yield的第一个版本和缺陷](#212-yield---------)
    + [2.1.2 Yield的第二个版本和缺陷](#212-yield---------)
    + [2.1.3 Yield的第三个版本](#213-yield------)
  * [2.2 用户级线程创建函数ThreadCreate()](#22----------threadcreate--)
- [3 (精华)工程上真正的实现用户级线程](#3------------------)
  * [3.1 环境配置](#31-----)
  * [3.2 实现目标](#32-----)
  * [3.3 程序执行过程中栈的图示](#33------------)
  * [3.4 代码](#34---)
  * [3.5 顺带收获](#35-----)
    + [3.6.1 为什么不把通用寄存器放在TCB中](#361-------------tcb-)
    + [3.6.2 为什么不用现代的编译器](#362------------)
    + [3.6.3 线程栈的排放内容是怎么想出来的](#363----------------)
    + [3.6.4 为什么在C语言中习惯把所有函数的声明放在开头](#364-----c-----------------)
    + [3.6.5 为什么要用while(...){...;Yield()}来测试？](#365------while----yield-------)

<small><i><a href='http://ecotrust-canada.github.io/markdown-toc/'>Table of contents generated with markdown-toc</a></i></small>

# 1 用户级线程的概念
线程是在一个地址空间下启动并交替执行的程序，用户级线程用用户管理。
# 2 用户级线程的设计和实现
## 2.1 用户级线程切换Yield()
线程1，线程2如下所示，在线程1中调用Yield()来切换到线程2
![在这里插入图片描述](https://img-blog.csdnimg.cn/20191119182618210.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L0VjdXN0X2FwcGxpZWRfbWF0aA==,size_16,color_FFFFFF,t_70)
进程的执行过程为
A() -> B() -> Yield() ->线程2-> C() -> D() -> Yield()
下面讨论如何实现Yield()

### 2.1.1 为什么先设计Yield()而不是ThreadCreate()
只要把一段程序做成可以用Yield切换进来的样子，并且CPU的进入点是这段程序的首地址，这段程序就可以执行了。所以创建一个用户级线程就是创建一个可以让CPU切换进去的初始样子。
### 2.1.2 Yield的第一个版本和缺陷
在B()中Yield()直接执行jmp 300。
```c
Yield(2){
    jmp 300;//300可以用TCB记录,jmp TCB(1).EIP
}
```
在C()中Yield()直接执行jmp 204。
```c
Yield(1){
	jmp 204;//204可以用TCB记录
}
```
此时进程的返回地址栈变化如下
![在这里插入图片描述](https://img-blog.csdnimg.cn/20191119183828440.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L0VjdXN0X2FwcGxpZWRfbWF0aA==,size_16,color_FFFFFF,t_70)
D中执行完Yield(1)之后，进入204往下执行，一直执行到B函数的“ } ”，执行ret指令，弹出的是404，发生了错误！(应该是回到A函数中继续执行)。所以，**直接jmp是行不通的！**

### 2.1.2 Yield的第二个版本和缺陷
上述问题显然是由于两个线程共用一个栈产生的，解决办法当然是给每个线程都分配一个栈。用线程的TCB来存储线程的esp(不用存储ebp，因为可以公用一个ebp)
两个栈的图示如下
![在这里插入图片描述](https://img-blog.csdnimg.cn/20191119190555526.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L0VjdXN0X2FwcGxpZWRfbWF0aA==,size_16,color_FFFFFF,t_70)
B中的Yield代码自然的想法就是不仅要切换PC到C的地址300去执行，还要修改当前esp为线程2的esp，代码如下
```c
Yield(2){
    tcb1.esp = esp;
    esp = tcb2.esp;
    jmp 300;//jmp esp.EIP
}
```
D中的Yield代码为
```c
Yield(1){
    tcb2.esp = esp;
    esp = tcb1.esp;
    jmp 204;//jmp esp.EIP
}
```
执行过程中的返回地址压栈如图所示
![在这里插入图片描述](https://img-blog.csdnimg.cn/20191119191726296.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L0VjdXN0X2FwcGxpZWRfbWF0aA==,size_16,color_FFFFFF,t_70)
此时又出现问题了，D()中执行Yield()时遇到jmp 204，便开始顺着204朝下执行，但是在遇到B的" } "时，执行ret，栈弹出的是204，又返回到了204执行，这出现了错误。这个错误的原因在于Yield的 } 没有被执行。回顾刚刚的过程，204被压栈是因为要去执行Yield，但是Yield的" } "并没有被执行。

### 2.1.3 Yield的第三个版本
解决的办法就是删去Yield()中的jmp 204，因为这样就可以使得Yield的" } "得到执行，最重要的，这个" } "与jmp 204有着同样的效果。
B中的yield代码如下
```c
Yield(2){
	tcb1.esp = esp;
	esp = tcb2.esp;
}
```
D中的yield代码如下
```c
Yield(1){
	tcb2.esp = esp;
	esp = tcb1.esp;
}
```
返回地址栈的过程如下
![在这里插入图片描述](https://img-blog.csdnimg.cn/20191119192816962.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L0VjdXN0X2FwcGxpZWRfbWF0aA==,size_16,color_FFFFFF,t_70)
总结如下:

 - 用户级线程的切换就是在切换位置上调用Yield()
 - Yield干的事情如下
   	 - 将当前的执行现场保存在当前线程的TCB中
   	 - 找到下个线程的的TCB的栈指针
   	 - 在下个线程的栈中弹出下个线程的执行现场
用C语言来描述如下
```c
Yield(){
	next = FindNext();
	push %eax
	push %ebx
	....
	mov %esp,TCB[current].esp
	mov TCB[current].esp,%esp
	....
	pop %ebx
	pop %eax
}
```
## 2.2 用户级线程创建函数ThreadCreate()
ThreadCreate(funcA)的核心功能如下：
1. 创建一个TCB，用来存储funcA的执行现场信息
2. 创建一个funcA的执行栈，栈的第一个压入的元素应该是funcA的起始地址，这样当别的线程执行Yield(A)时候，使用“ } ”的ret指令便可以开始执行funcA
3. 把TCB和funcA的栈关联起来
![在这里插入图片描述](https://img-blog.csdnimg.cn/20191119205541185.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L0VjdXN0X2FwcGxpZWRfbWF0aA==,size_16,color_FFFFFF,t_70)
ThreadCreate的核心代码如下
```c
void ThreadCreate(A){
	TCB * tcb = malloc();
	Stack * stack = malloc();
	*stack = A;
	tcb.esp = stack;
}
```
# 3 (精华)工程上真正的实现用户级线程
## 3.1 环境配置
名称 | 配置
--- | ---
操作系统 | win 10(64位)
编译器 | MingGW 3.2.3(32位 老古董了)
## 3.2 实现目标
在屏幕上交替打印出A和B。即要让以下代码
```c
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
	int t2 = ThreadCreate(B,arg2);
	Run();
	return 0;
}

```
有以下的效果
![在这里插入图片描述](https://img-blog.csdnimg.cn/20191124114256998.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L0VjdXN0X2FwcGxpZWRfbWF0aA==,size_16,color_FFFFFF,t_70)
## 3.3 程序执行过程中栈的图示
![在这里插入图片描述](https://img-blog.csdnimg.cn/20191124121718910.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L0VjdXN0X2FwcGxpZWRfbWF0aA==,size_16,color_FFFFFF,t_70)
![在这里插入图片描述](https://img-blog.csdnimg.cn/20191124121749462.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L0VjdXN0X2FwcGxpZWRfbWF0aA==,size_16,color_FFFFFF,t_70)![在这里插入图片描述](https://img-blog.csdnimg.cn/20191124121803360.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L0VjdXN0X2FwcGxpZWRfbWF0aA==,size_16,color_FFFFFF,t_70)
![在这里插入图片描述](https://img-blog.csdnimg.cn/20191124121842802.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L0VjdXN0X2FwcGxpZWRfbWF0aA==,size_16,color_FFFFFF,t_70)
![在这里插入图片描述](https://img-blog.csdnimg.cn/20191124121855910.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L0VjdXN0X2FwcGxpZWRfbWF0aA==,size_16,color_FFFFFF,t_70)
![在这里插入图片描述](https://img-blog.csdnimg.cn/20191124121915739.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L0VjdXN0X2FwcGxpZWRfbWF0aA==,size_16,color_FFFFFF,t_70)
![在这里插入图片描述](https://img-blog.csdnimg.cn/20191124121931388.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L0VjdXN0X2FwcGxpZWRfbWF0aA==,size_16,color_FFFFFF,t_70)
![在这里插入图片描述](https://img-blog.csdnimg.cn/20191124121955687.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L0VjdXN0X2FwcGxpZWRfbWF0aA==,size_16,color_FFFFFF,t_70)
![在这里插入图片描述](https://img-blog.csdnimg.cn/20191124124038627.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L0VjdXN0X2FwcGxpZWRfbWF0aA==,size_16,color_FFFFFF,t_70)
![在这里插入图片描述](https://img-blog.csdnimg.cn/20191124124316322.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L0VjdXN0X2FwcGxpZWRfbWF0aA==,size_16,color_FFFFFF,t_70)
![在这里插入图片描述](https://img-blog.csdnimg.cn/2019112412433514.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L0VjdXN0X2FwcGxpZWRfbWF0aA==,size_16,color_FFFFFF,t_70)
![在这里插入图片描述](https://img-blog.csdnimg.cn/20191124124355335.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L0VjdXN0X2FwcGxpZWRfbWF0aA==,size_16,color_FFFFFF,t_70)
![在这里插入图片描述](https://img-blog.csdnimg.cn/20191124124414898.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L0VjdXN0X2FwcGxpZWRfbWF0aA==,size_16,color_FFFFFF,t_70)
![在这里插入图片描述](https://img-blog.csdnimg.cn/20191125112702148.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L0VjdXN0X2FwcGxpZWRfbWF0aA==,size_16,color_FFFFFF,t_70)
![在这里插入图片描述](https://img-blog.csdnimg.cn/20191125112811164.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L0VjdXN0X2FwcGxpZWRfbWF0aA==,size_16,color_FFFFFF,t_70)
![在这里插入图片描述](https://img-blog.csdnimg.cn/20191125112827359.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L0VjdXN0X2FwcGxpZWRfbWF0aA==,size_16,color_FFFFFF,t_70)
![在这里插入图片描述](https://img-blog.csdnimg.cn/20191125112839416.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L0VjdXN0X2FwcGxpZWRfbWF0aA==,size_16,color_FFFFFF,t_70)
![在这里插入图片描述](https://img-blog.csdnimg.cn/20191125113100849.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L0VjdXN0X2FwcGxpZWRfbWF0aA==,size_16,color_FFFFFF,t_70)
![在这里插入图片描述](https://img-blog.csdnimg.cn/20191125113119693.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L0VjdXN0X2FwcGxpZWRfbWF0aA==,size_16,color_FFFFFF,t_70)
![在这里插入图片描述](https://img-blog.csdnimg.cn/20191125113132220.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L0VjdXN0X2FwcGxpZWRfbWF0aA==,size_16,color_FFFFFF,t_70)
![在这里插入图片描述](https://img-blog.csdnimg.cn/20191125113152952.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L0VjdXN0X2FwcGxpZWRfbWF0aA==,size_16,color_FFFFFF,t_70)
![在这里插入图片描述](https://img-blog.csdnimg.cn/20191125113218611.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L0VjdXN0X2FwcGxpZWRfbWF0aA==,size_16,color_FFFFFF,t_70)
![在这里插入图片描述](https://img-blog.csdnimg.cn/20191125113237634.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L0VjdXN0X2FwcGxpZWRfbWF0aA==,size_16,color_FFFFFF,t_70)
![在这里插入图片描述](https://img-blog.csdnimg.cn/20191125113251493.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L0VjdXN0X2FwcGxpZWRfbWF0aA==,size_16,color_FFFFFF,t_70)
## 3.4 代码
```c
//TCB.h
//TCB.h
typedef struct TCB{
	int t_id;
	int state;
	void* pStack;
	long esp;
	long ebp;
} TCB;

```
```c
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

```
## 3.5 顺带收获
### 3.6.1 为什么不把通用寄存器放在TCB中
笔者最开始的时候是把所有的通用寄存器放在TCB中，但是在内嵌汇编编译__asm__的时候会产生错误，提升没有可用的通用寄存器，故只好以压栈的方式来存储。
### 3.6.2 为什么不用现代的编译器
因为现代的编译器不支持在一个函数的结束部分修改ebp，会产生cannot bp here错误.
```c
A(){
    __asm__("movl %%esp,%%ebp":::"ebp");
}
```
这在GCC 4以后编译是通不过的
### 3.6.3 线程栈的排放内容是怎么想出来的
一个是根据C语言的参数传递约定，二是在大脑中模仿程序的执行过程，一个一个试出来的，也走了很多弯路。
### 3.6.4 为什么在C语言中习惯把所有函数的声明放在开头
```c
A(){
	B();
}
B(){
	C();
}
C(){
	D();
}
```
直接编译的时候会提示找不到B，找不到C，找不到D，为了排版方便，统一把所有函数的声明放在最前面，用到的时候再去找定义。
### 3.6.5 为什么要用while(...){...;Yield()}来测试？
```c
void A(){
	while(1){
		printf("A");
		Yield();
	}
}
void B(){
	while(1){
		printf("B");
		Yield();
	}
}
```
等价于
```c
void A(){
	printf("A");
	Yield();
	printf("A");
	Yield();
	printf("A");
	Yield();
	printf("A");
	Yield();
	....
	....
}
void B(){
	printf("B");
	Yield();
	printf("B");
	Yield();
	printf("B");
	Yield();
	printf("B");
	Yield();
	....
	....
}
```
这样就是每一个线程执行一句话，然后切换过去。
