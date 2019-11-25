//TCB.h
typedef struct TCB{
	int t_id;
	int state;
	void* pStack;
	long esp;
	long ebp;
} TCB;
