#include <geekos/syscall.h>
#include <geekos/errno.h>
#include <geekos/kthread.h>
#include <geekos/int.h>
#include <geekos/elf.h>
#include <geekos/malloc.h>
#include <geekos/screen.h>
#include <geekos/keyboard.h>
#include <geekos/string.h>
#include <geekos/user.h>
#include <geekos/timer.h>
#include <geekos/vfs.h>
#include <geekos/signal.h>
#include <geekos/sem.h>
#include <geekos/projects.h>
#include <geekos/smp.h>

#define NameSize 30
#define SemaphoreSize 30

extern int Copy_User_String(ulong_t uaddr, ulong_t len, ulong_t maxLen, char** pStr);

struct Semaphore
{
    int value;
    int user;
    char* name;
    struct Thread_Queue waitq;
};

static struct Semaphore semaphoreList[SemaphoreSize] = { 0 };

int Sys_Open_Semaphore(struct Interrupt_State* state)
{
    char* name = 0;
    int SID = 0;
    int rtn;
    int check = 0;
    int length = state->ebx;

    if ((state->ecx > NameSize) || ((int)state->ecx <= 0) || ((int)state->edx < 0))
        return EINVALID;

    rtn = Copy_User_String(length, state->ecx, NameSize, &name);

    if (rtn != 0) 
        return rtn;

    while (semaphoreList[SID].name != 0)
    {
        if (strncmp(semaphoreList[SID].name, name, state->ecx) == 0)
        {
            Free(name);
            rtn = SID;
            semaphoreList[SID].user += 1;
            check += 1;
        }

        SID += 1;

        if (SemaphoreSize == SID)
        {
            Free(name);
            rtn = ENOSPACE;
            check += 1;
        }
    }

    if (check == 0)
    {
        semaphoreList[SID].value = state->edx;
        semaphoreList[SID].user = 1;
        semaphoreList[SID].name = name;

        Clear_Thread_Queue(&semaphoreList[SID].waitq);
        rtn = SID;
    }

    return rtn;
}

int Sys_P(struct Interrupt_State* state)
{
    int length = state->ebx;

    if (((int)length < 0) || (length >= SemaphoreSize) || (semaphoreList[length].name == 0))
        return EINVALID;

    bool iflag = Begin_Int_Atomic();

    semaphoreList[length].value -= 1;

    if (semaphoreList[length].value < 0)
    {
        End_Int_Atomic(iflag);
        Wait(&semaphoreList[length].waitq);
    }

    End_Int_Atomic(iflag);

    return 0;
}

int Sys_V(struct Interrupt_State* state)
{
    int length = state->ebx;
    if (((int)length < 0) || (length >= SemaphoreSize) || (semaphoreList[length].name == 0))
        return EINVALID;

    bool iflag = Begin_Int_Atomic();

    semaphoreList[length].value += 1;

    if (Is_Thread_Queue_Empty(&semaphoreList[length].waitq) == 0)
    {
        Wake_Up_One(&semaphoreList[length].waitq);
    }

    End_Int_Atomic(iflag);

    return 0;
}

int Sys_Close_Semaphore(struct Interrupt_State* state)
{
    int length = state->ebx;

    if (((int)length < 0) || (length >= SemaphoreSize) || (semaphoreList[length].name == 0))
        return EINVALID;

    bool iflag = Begin_Int_Atomic();

    semaphoreList[length].user -= 1;

    if (semaphoreList[length].user == 0)
    {
        Free(semaphoreList[length].name);
        semaphoreList[length].name = 0;
    }

    End_Int_Atomic(iflag);

    return 0;
}