// os345interrupts.c - pollInterrupts	08/08/2013
// ***********************************************************************
// **   DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER   **
// **                                                                   **
// ** The code given here is the basis for the BYU CS345 projects.      **
// ** It comes "as is" and "unwarranted."  As such, when you use part   **
// ** or all of the code, it becomes "yours" and you are responsible to **
// ** understand any algorithm or method presented.  Likewise, any      **
// ** errors or problems become your responsibility to fix.             **
// **                                                                   **
// ** NOTES:                                                            **
// ** -Comments beginning with "// ??" may require some implementation. **
// ** -Tab stops are set at every 3 spaces.                             **
// ** -The function API's in "OS345.h" should not be altered.           **
// **                                                                   **
// **   DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER   **
// ***********************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <time.h>
#include <assert.h>

#include "os345.h"
#include "os345config.h"
#include "os345signals.h"

// **********************************************************************
//	local prototypes
//
void pollInterrupts(void);
static void keyboard_isr(void);
static void timer_isr(void);

// **********************************************************************
// **********************************************************************
// global semaphores

extern Semaphore* keyboard;				// keyboard semaphore
extern Semaphore* charReady;				// character has been entered
extern Semaphore* inBufferReady;			// input buffer ready semaphore

extern Semaphore* tics1sec;				// 1 second semaphore
extern Semaphore* tics10sec;				// 1 second semaphore
extern Semaphore* tics10thsec;				// 1/10 second semaphore

extern char inChar;				// last entered character
extern int charFlag;				// 0 => buffered input
extern int inBufIndx;				// input pointer into input buffer
extern char inBuffer[INBUF_SIZE+1];	// character input buffer

extern time_t oldTime1;					// old 1sec time
extern time_t oldTime10;					// old 10sec time
extern clock_t myClkTime;
extern clock_t myOldClkTime;

extern int pollClock;				// current clock()
extern int lastPollClock;			// last pollClock

extern int superMode;						// system mode

extern TCB tcb[];
extern CommandNode* head;
extern CommandNode* tail;

CommandNode* current;

extern int commandPos;


// **********************************************************************
// **********************************************************************
// simulate asynchronous interrupts by polling events during idle loop
//
void pollInterrupts(void)
{
	// check for task monopoly
	pollClock = clock();
	assert("Timeout" && ((pollClock - lastPollClock) < MAX_CYCLES));
	lastPollClock = pollClock;

	// check for keyboard interrupt
	if ((inChar = GET_CHAR) > 0)
	{
	  keyboard_isr();
	}

	// timer interrupt
	timer_isr();

	return;
} // end pollInterrupts


// **********************************************************************
// keyboard interrupt service routine
//
static void keyboard_isr()
{
	// assert system mode
	assert("keyboard_isr Error" && superMode);

	semSignal(charReady);					// SIGNAL(charReady) (No Swap)
	if (charFlag == 0)
	{
		switch (inChar)
		{
			case '\r':
			case '\n':
			{
				inBufIndx = 0;				// EOL, signal line ready
				commandPos = 0;
				current = NULL;
				semSignal(inBufferReady);	// SIGNAL(inBufferReady)
				break;
			}
			case 0x12:						// ^r
			{
				for (int taskId=0; taskId<MAX_TASKS; taskId++)
				{
					if(tcb[taskId].name) {
						tcb[taskId].signal &= ~mySIGSTOP;
						tcb[taskId].signal &= ~mySIGTSTP;
					}
				}
				sigSignal(ALL_TID, mySIGCONT);
				break;
			}
			case 0x17:						// ^w
			{
				sigSignal(ALL_TID,mySIGTSTP);
				break;
			}
			case 0x18:						// ^x
			{
				inBufIndx = 0;
				inBuffer[0] = 0;
				sigSignal(0, mySIGINT);		// interrupt task 0
				semSignal(inBufferReady);	// SEM_SIGNAL(inBufferReady)
				break;
			}
			case 0x1B:
			{
				char temp = GET_CHAR; // get rid of extra special character
				temp = GET_CHAR; // used to determine what the key is
				switch (temp) {
					case 0x33:
					{
						temp = GET_CHAR;

						if(commandPos < inBufIndx) {
							for (size_t i = commandPos; i < inBufIndx; i++) {
								inBuffer[i] = inBuffer[i + 1];
								printf("%c", inBuffer[i]);
							}
							printf(" ");
							for (size_t i = commandPos; i < inBufIndx; i++) {
								printf("\b");
							}
							inBuffer[--inBufIndx] = 0;
						}

						break;
					}
					case 0x41://up
					{
						if(current == NULL)
						{
							current = head;
						}
						else if (current == tail)
						{
							break;
						}
						else
						{
							current = current->prev;
						}
						if(current == NULL) {
							break;
						}

						if(commandPos != inBufIndx)
						{
							while(commandPos < inBufIndx) {
								commandPos++;
								printf(" ");
							}
						}
						while(inBufIndx-- > 0) {
							commandPos--;
							printf("\b \b");
						}
						strcpy(inBuffer, current->command);
						inBufIndx = strlen(current->command);
						commandPos = inBufIndx;
						inBuffer[inBufIndx] = 0;
						printf("%s", current->command);
						break;
					}
					case 0x42://down
					{
						if(current != NULL)
						{
							if(commandPos != inBufIndx)
							{
								while(commandPos < inBufIndx) {
									commandPos++;
									printf(" ");
								}
							}
							while(inBufIndx-- > 0) {
								commandPos--;
								printf("\b \b");
							}

							current = current->next;
							if(current == NULL) {
								commandPos = 0;
								inBufIndx = 0;
								inBuffer[inBufIndx] = 0;
							}
							else
							{
								strcpy(inBuffer, current->command);
								inBufIndx = strlen(current->command);
								commandPos = inBufIndx;
								inBuffer[inBufIndx] = 0;
								printf("%s", current->command);
							}
						}
						break;
					}
					case 0x43://right
					{
						if(commandPos < inBufIndx)
						{
							printf("%c",inBuffer[commandPos++]);
						}
						break;
					}
					case 0x44://left
					{
						if(commandPos > 0)
						{
							commandPos--;
							printf("\b");
						}
						break;
					}
					case 0x46:
					{
						while (commandPos < inBufIndx)
						{
							printf("%c",inBuffer[commandPos++]);
						}
						break;
					}
					case 0x48:
					{
						while (commandPos > 0)
						{
							commandPos--;
							printf("\b");
						}
						break;
					}
				}
				break;
			}
			case 0x7F:
			{
				if (commandPos > 0)
				{
					if(commandPos == inBufIndx)
					{
						printf("\b \b");
					}
					else {
						printf("\b");
						for (size_t i = commandPos; i < inBufIndx; i++) {
							inBuffer[i - 1] = inBuffer[i];
							printf("%c", inBuffer[i - 1]);
						}
						printf(" ");
						for (size_t i = commandPos; i <= inBufIndx; i++) {
							printf("\b");
						}
					}
					inBuffer[--inBufIndx] = 0;
					commandPos--;
					/*
						steps:
						while commandpos
					*/
				}
				break;
			}
			default:
			{
				if(commandPos < inBufIndx)
				{
					printf(" ");
					for (size_t i = inBufIndx; i > commandPos; i--) {
						inBuffer[i] = inBuffer[i - 1];
					}
					for (size_t i = commandPos + 1; i <= inBufIndx; i++) {
						printf("%c", inBuffer[i]);
					}
					for (size_t i = inBufIndx; i >= commandPos; i--) {
						printf("\b");
					}
				}
				inBuffer[commandPos++] = inChar;
				inBufIndx++;
				inBuffer[inBufIndx] = 0;
				printf("%c", inChar);		// echo character
			}
		}
	}
	else
	{
		// single character mode
		inBufIndx = 0;
		inBuffer[inBufIndx] = 0;
	}
	return;
} // end keyboard_isr


// **********************************************************************
// timer interrupt service routine
//
static void timer_isr()
{
	time_t currentTime;						// current time

	// assert system mode
	assert("timer_isr Error" && superMode);

	// capture current time
  	time(&currentTime);

  	// one second timer
  	if ((currentTime - oldTime1) >= 1)
  	{
		// signal 1 second
  	   semSignal(tics1sec);
		oldTime1 += 1;
  	}

    if ((currentTime - oldTime10) >= 10)
  	{
      // signal 10 second
      semSignal(tics10sec);
      oldTime10 += 10;
  	}

	// sample fine clock
	myClkTime = clock();
	if ((myClkTime - myOldClkTime) >= ONE_TENTH_SEC)
	{
		myOldClkTime = myOldClkTime + ONE_TENTH_SEC;   // update old
		semSignal(tics10thsec);
	}

	// ?? add other timer sampling/signaling code here for project 2

	return;
} // end timer_isr
