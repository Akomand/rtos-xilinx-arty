#include "xil_printf.h"
#include "xparameters.h"
#include "xtmrctr.h"
#include "xil_exception.h"
#include "xintc.h"
#include <stdio.h>
#include <stdbool.h>


#define TMRCTR_DEVICE_ID XPAR_TMRCTR_0_DEVICE_ID
#define TMRCTR_INTERRUPT_ID XPAR_INTC_0_TMRCTR_0_VEC_ID
#define INTC_DEVICE_ID XPAR_INTC_0_DEVICE_ID

#define TIMER_0 0

#define LOAD_VALUE 416667

void TimerCounterHandler(void *CallBackRef, u8 TmrCtrNumber);
void TmrCtrDisableIntr(XIntc *IntcInstancePtr, u16 IntrId);
void executionFailed();


typedef enum {RED, GREEN, FLASH_RED_START, FLASH_RED_END} State;
typedef enum {TASK_CHOOSE, TASK_GREEN, TASK_RED_BLINK_START, TASK_RED, TASK_RED_BLINK_END} Task;


// Create state variables
State state, next_state;


// Task Functions
void taskChoose(void* nodata);
void taskGreen(volatile int *RGBLedsData, volatile int *buttonsData, int oldButtonsData);
void taskRedBlinkStart(volatile int *RGBLedsData, volatile int *buttonsData, int oldButtonsData);
void taskRedSolid(volatile int *RGBLedsData, volatile int *buttonsData, int oldButtonsData);
void taskRedBlinkEnd(volatile int *RGBLedsData, volatile int *buttonsData, int oldButtonsDataa);


XIntc InterruptController;
XTmrCtr TimerCounter;


int count = 0;
int msCount = 0;


#define PUSH_BTNS_BASE 0x40000008
#define RGB_LEDS_BASE 0x40010000


#define PUSH_BTNS_REG (int *)(PUSH_BTNS_BASE)
#define RGB_LEDS_REG (int *)(RGB_LEDS_BASE)


// Task Structure
typedef struct {
	void (*taskPtr)(volatile int *, volatile int *, int);
	void *taskDataPtr;
	bool taskReady;
} TCB;


TCB* queue[5];


// TaskChoose
void taskChoose(void* noData) {
	queue[TASK_CHOOSE]->taskReady = 0;
	switch(state) {
		case GREEN:
			queue[1]->taskReady = true;
			break;
		case FLASH_RED_START:
			queue[2]->taskReady = true;
			break;
		case RED:
			queue[3]->taskReady = true;
			break;
		case FLASH_RED_END:
			queue[4]->taskReady = true;
			break;

	}
}

// TaskGreen
void taskGreen(volatile int *RGBLedsData, volatile int *buttonsData, int oldButtonsData) {
	queue[TASK_GREEN]->taskReady = false;
//	oldButtonsData = *buttonsData;

	*RGBLedsData = 0x492;
	if (*buttonsData != oldButtonsData){
		next_state = FLASH_RED_START;
	}
	else{
		next_state = GREEN;
	}
//	next_state = FLASH_RED_START;
	state = next_state;

}

// TaskRedBlinkStart
void taskRedBlinkStart(volatile int *RGBLedsData, volatile int *buttonsData, int oldButtonsData) {
	oldButtonsData = *buttonsData;
	queue[TASK_RED_BLINK_START]->taskReady = false;
	count = 0;
	while(count < 12)
	{
		oldButtonsData = *buttonsData;
		if (count % 2 == 0){
			*RGBLedsData = 0x924;
		}
		else{
			*RGBLedsData = 0x000;
		}
		if(*buttonsData != oldButtonsData){
			count = 0;
		}
	}
	count = 0;
	next_state = RED;
	state = next_state;

}

// TaskRedSolid
void taskRedSolid(volatile int *RGBLedsData, volatile int *buttonsData, int oldButtonsData) {
	queue[TASK_RED]->taskReady = false;
//	oldButtonsData = *buttonsData;

	while(count < 8)
	{
		*RGBLedsData = 0x924;
	}
	count = 0;
	next_state = FLASH_RED_END; // next state after RED
	state = next_state;
}

// TaskRedBlinkEnd
void taskRedBlinkEnd(volatile int *RGBLedsData, volatile int *buttonsData, int oldButtonsData) {
//	oldButtonsData = *buttonsData;
	queue[TASK_RED_BLINK_END]->taskReady = false;
	while(count < 12)
	{
		oldButtonsData = *buttonsData;
		if (count % 2 == 0){
			*RGBLedsData = 0x924;
		}
		else{
			*RGBLedsData = 0x000;
		}
	}
	count = 0;
	next_state = GREEN;
	state = next_state;
}


void setup()
{
	int status;
	    // Initialize the timer counter instance
	    status = XTmrCtr_Initialize(&TimerCounter, TMRCTR_DEVICE_ID);
	    if (status != XST_SUCCESS)
	    {
	        xil_printf("Failed to initialize the timer! Execution stopped.\n");
	        executionFailed();
	    }

	    // Verifies the specified timer is setup correctly in hardware/software
	    status = XTmrCtr_SelfTest(&TimerCounter, 1);
	    if (status != XST_SUCCESS)
	    {
	        xil_printf("Testing timer operation failed! Execution stopped.\n");
	        executionFailed();
	    }

	    // Initialize the interrupt controller instance
	    status = XIntc_Initialize(&InterruptController, INTC_DEVICE_ID);
	    if (status != XST_SUCCESS)
	    {
	        xil_printf("Failed to initialize the interrupt controller! Execution stopped.\n");
	        executionFailed();
	    }

	    // Connect a timer handler that will be called when an interrupt occurs
	    status = XIntc_Connect( &InterruptController,
	                            TMRCTR_INTERRUPT_ID,
	                            (XInterruptHandler)XTmrCtr_InterruptHandler,
	                            (void *)&TimerCounter);
	    if (status != XST_SUCCESS)
	    {
	        xil_printf("Failed to connect timer handler! Execution stopped.\n");
	        executionFailed();
	    }

	    // Start the interrupt controller
	    status = XIntc_Start(&InterruptController, XIN_REAL_MODE);
	    if (status != XST_SUCCESS)
	    {
	        xil_printf("Failed to start interrupt controller! Execution stopped.\n");
	        executionFailed();
	    }


	    // Enable interrupts and the exception table
	    XIntc_Enable(&InterruptController, TMRCTR_INTERRUPT_ID);

	    Xil_ExceptionInit();

		// Register the interrupt controller handler with the exception table.
	    Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
	                                 (Xil_ExceptionHandler)XIntc_InterruptHandler,
	                                 &InterruptController);

	    Xil_ExceptionEnable();


	    // Set the handler (function pointer) that we want to execute when the
	    // interrupt occurs
	    XTmrCtr_SetHandler(&TimerCounter, TimerCounterHandler, &TimerCounter);


	    // Set our timer options (setting TCSR register indirectly through Xil API)
	    XTmrCtr_SetOptions(&TimerCounter, TIMER_0,
	                       XTC_INT_MODE_OPTION | XTC_DOWN_COUNT_OPTION | XTC_AUTO_RELOAD_OPTION);


	    // Set what value the timer should reset/init to (setting TLR indirectly)
	    XTmrCtr_SetResetValue(&TimerCounter, TIMER_0, LOAD_VALUE);


	    // Start the timer
	    XTmrCtr_Start(&TimerCounter, TIMER_0);
}


void executionFailed()
{
    *RGB_LEDS_REG = 0b001001001001; // display all red LEDs if fail state occurs
    while(1);
}


void TimerCounterHandler(void *CallBackRef, u8 TmrCtrNumber)
{
    XTmrCtr *InstancePtr = (XTmrCtr *)CallBackRef;
    /*
	 * Check if the timer counter has expired, checking is not necessary
	 * since that's the reason this function is executed, this just shows
	 * how the callback reference can be used as a pointer to the instance
	 * of the timer counter that expired, increment a shared variable so
	 * the main thread of execution can see the timer expired
	 */
    if (XTmrCtr_IsExpired(InstancePtr, TmrCtrNumber))
    {
    	msCount += LOAD_VALUE;
    	if(msCount >= 41666667){
    		count++;
    		msCount = 0;
    	}
    	queue[TASK_CHOOSE]->taskReady = true;
    }
}


void TmrCtrDisableIntr(XIntc *IntcInstancePtr, u16 IntrId)
{
    // Disable the interrupt for the timer counter
    XIntc_Disable(IntcInstancePtr, IntrId);
    return;
}



int main(void)
{
	volatile int *buttonsData = PUSH_BTNS_REG;
	volatile int *buttonsTri = PUSH_BTNS_REG + 1;
	volatile int *RGBLedsData = RGB_LEDS_REG;
	volatile int *RGBLedsTri = RGB_LEDS_REG + 1;


	*buttonsTri = 0xF;
	*RGBLedsTri = 0x0;


	queue[TASK_CHOOSE] = malloc(sizeof(TCB));
	queue[TASK_CHOOSE]->taskReady = true;
	queue[TASK_CHOOSE]->taskPtr = taskChoose;
	queue[TASK_CHOOSE]->taskDataPtr = NULL;

	queue[TASK_RED] = malloc(sizeof(TCB));
	queue[TASK_RED]->taskReady = false;
	queue[TASK_RED]->taskPtr = taskRedSolid;
	queue[TASK_RED]->taskDataPtr = NULL;

	queue[TASK_GREEN] = malloc(sizeof(TCB));
	queue[TASK_GREEN]->taskReady = false;
	queue[TASK_GREEN]->taskPtr = taskGreen;
	queue[TASK_GREEN]->taskDataPtr = NULL;

	queue[TASK_RED_BLINK_START] = malloc(sizeof(TCB));
	queue[TASK_RED_BLINK_START]->taskReady = false;
	queue[TASK_RED_BLINK_START]->taskPtr = taskRedBlinkStart;
	queue[TASK_RED_BLINK_START]->taskDataPtr = NULL;

	queue[TASK_RED_BLINK_END] = malloc(sizeof(TCB));
	queue[TASK_RED_BLINK_END]->taskReady = false;
	queue[TASK_RED_BLINK_END]->taskPtr = taskRedBlinkEnd;
	queue[TASK_RED_BLINK_END]->taskDataPtr = NULL;


	int oldButtonsData = 0;
	state = GREEN;

	// Call setup method to setup timer and interrupt controller
	int i = 0;
	TCB* aTCBPtr;


	setup();

	// Infinitely loop
	print("Beginning infinite loop:\r\n");
	while (1)
	{
		if(!(queue[i]->taskReady)) {
			i = (i+1)%5;
			continue;
		}

		aTCBPtr = queue[i];
		aTCBPtr->taskPtr(&*RGBLedsData, &*buttonsData, oldButtonsData);

		i = (i+1)%5;
	}
	return 0;
}
