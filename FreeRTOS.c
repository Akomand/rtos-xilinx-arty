// Author: Aneesh Komanduri
// FreeRTOS implementation of Fayetteville Bike Crossing

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* Xilinx includes. */
#include "xil_printf.h"
#include "xparameters.h"

/* Traffic Signal States */
typedef enum {
	GREEN,
	RED_BLINK_START,
	RED_SOLID,
	RED_BLINK_END
} State;

State next_state;
State state = GREEN; // Shared resource. Mutex driven.
SemaphoreHandle_t state_mutex;

#define TMRCTR_DEVICE_ID XPAR_TMRCTR_0_DEVICE_ID
#define TMRCTR_INTERRUPT_ID XPAR_INTC_0_TMRCTR_0_VEC_ID
#define INTC_DEVICE_ID XPAR_INTC_0_DEVICE_ID

#define TIMER_0 0

#define LOAD_VALUE 416667

#define PUSH_BTNS_BASE 0x40000008
#define RGB_LEDS_BASE 0x40010000


#define PUSH_BTNS_REG (int *)(PUSH_BTNS_BASE)
#define RGB_LEDS_REG (int *)(RGB_LEDS_BASE)

volatile int *buttonsData = PUSH_BTNS_REG;
volatile int *buttonsTri = PUSH_BTNS_REG + 1;
volatile int *RGBLedsData = RGB_LEDS_REG;
volatile int *RGBLedsTri = RGB_LEDS_REG + 1;

int oldButtonsData = 0;
int count = 0;

/* FreeRTOS Tasks */
TaskHandle_t SupervisorTaskHandle = NULL;
void taskChoose(void* nodata);

TaskHandle_t greenHandler = NULL;
void taskGreen(void* nodata);

TaskHandle_t RBSHandler = NULL;
void taskRedBlinkStart(void* nodata);

TaskHandle_t redHandler = NULL;
void taskRedSolid(void* nodata);

TaskHandle_t RBEHandler = NULL;
void taskRedBlinkEnd(void* nodata);


/* Main Function */
int main(void)
{

	 // Setup and init code
	 xTaskCreate(taskChoose, "Supervisor", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, &SupervisorTaskHandle);
	 xTaskCreate(taskGreen, "taskGreen", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, &greenHandler);
	 xTaskCreate(taskRedBlinkStart, "taskRedBlinkStart", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, &RBSHandler);
	 xTaskCreate(taskRedSolid, "taskRedSolid", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, &redHandler);
	 xTaskCreate(taskRedBlinkEnd, "taskRedBlinkEnd", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, &RBEHandler);

	 // Suspend all state tasks to allow supervisor to run
	 vTaskSuspend(greenHandler);
	 vTaskSuspend(RBSHandler);
	 vTaskSuspend(redHandler);
	 vTaskSuspend(RBEHandler);

	 state_mutex = xSemaphoreCreateMutex();

	// Start FreeRTOS Kernel
	 vTaskStartScheduler();

	return 0;
}


// TaskChoose
void taskChoose(void* noData) {

	while(1) {
		if( xSemaphoreTake(state_mutex, 10) )
		{
			// Resume State Thread we want to run
			switch(state) {
				case GREEN:
					vTaskSuspend(RBEHandler);
					vTaskResume(greenHandler);
					break;
				case RED_BLINK_START:
					vTaskSuspend(greenHandler);
					vTaskResume(RBSHandler);
					break;
				case RED_SOLID:
					vTaskSuspend(RBSHandler);
					vTaskResume(redHandler);
					break;
				case RED_BLINK_END:
					vTaskSuspend(redHandler);
					vTaskResume(RBEHandler);
					break;

			}

			// Release lock
			xSemaphoreGive(state_mutex);
		}
		// Ensure preemption
		vTaskDelay(pdMS_TO_TICKS(100));
	}
}

// TaskGreen
void taskGreen(void* nodata) {
	while(1) {
		*RGBLedsData = 0x492;

		if( xSemaphoreTake(state_mutex, 10) )
		{
			if (*buttonsData != oldButtonsData){
				state = RED_BLINK_START;
			}
			else{
				state = GREEN;
			}
			xSemaphoreGive(state_mutex);
		}
	}
}

// TaskRedBlinkStart
void taskRedBlinkStart(void* nodata) {
	while(1) {
		oldButtonsData = *buttonsData;
		count = 0;
		while(count < 12)
		{
			if (count % 2 == 0){
				*RGBLedsData = 0x924;
			}
			else{
				*RGBLedsData = 0x000;
			}
			if(*buttonsData != oldButtonsData){
				count = 0;
			}
			oldButtonsData = *buttonsData;
			count++;
			vTaskDelay(pdMS_TO_TICKS(500));
		}


		count = 0;
		if( xSemaphoreTake(state_mutex, 10) )
		{
			state = RED_SOLID;
			xSemaphoreGive(state_mutex);
		}
	}

}

// TaskRedSolid
void taskRedSolid(void* nodata) {
	while(1) {
		while(count < 8)
		{
			*RGBLedsData = 0x924;
			count++;
			vTaskDelay(pdMS_TO_TICKS(500));
		}


		count = 0;
		 // next state after RED
		if( xSemaphoreTake(state_mutex, 10) )
			{
				state = RED_BLINK_END;
				xSemaphoreGive(state_mutex);
			}
		}
	}

// TaskRedBlinkEnd
void taskRedBlinkEnd(void* nodata) {

	while(1) {
		while(count < 12)
		{
			oldButtonsData = *buttonsData;
			if (count % 2 == 0){
				*RGBLedsData = 0x924;
			}
			else{
				*RGBLedsData = 0x000;
			}
			count++;
			vTaskDelay(pdMS_TO_TICKS(500));
		}

		count = 0;
		if( xSemaphoreTake(state_mutex, 10) )
		{
			state = GREEN;

			xSemaphoreGive(state_mutex);
		}
	}
}

