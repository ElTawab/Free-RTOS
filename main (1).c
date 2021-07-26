/*
 * This file is part of the µOS++ distribution.
 *   (https://github.com/micro-os-plus)
 * Copyright (c) 2014 Liviu Ionescu.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

// ----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include "diag/trace.h"

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "semphr.h"
#define CCM_RAM __attribute__((section(".ccmram")))

// ----------------------------------------------------------------------------

#include "led.h"

#define BLINK_PORT_NUMBER         (3)
#define BLINK_PIN_NUMBER_GREEN    (12)
#define BLINK_PIN_NUMBER_ORANGE   (13)
#define BLINK_PIN_NUMBER_RED      (14)
#define BLINK_PIN_NUMBER_BLUE     (15)
#define BLINK_ACTIVE_LOW          (false)
#define Queue_size                 (2) //size of queue
#define receiver_timevalue        (200)
struct led blinkLeds[4];

// ----------------------------------------------------------------------------
/*-----------------------------------------------------------*/

/*
 * The LED timer callback function.  This does nothing but switch the red LED
 * off.
 */

void SenderTimerCallback( TimerHandle_t xTimer );
 void ReceiverTimerCallback( TimerHandle_t xTimer );

/*-----------------------------------------------------------*/

/* The LED software timer.  This uses vLEDTimerCallback() as its callback
 * function.
 */


/*-----------------------------------------------------------*/
// ----------------------------------------------------------------------------
//
// Semihosting STM32F4 empty sample (trace via DEBUG).
//
// Trace support is enabled by adding the TRACE macro definition.
// By default the trace messages are forwarded to the DEBUG output,
// but can be rerouted to any device or completely suppressed, by
// changing the definitions required in system/src/diag/trace-impl.c
// (currently OS_USE_TRACE_ITM, OS_USE_TRACE_SEMIHOSTING_DEBUG/_STDOUT).
//

// ----- main() ---------------------------------------------------------------

// Sample pragmas to cope with warnings. Please note the related line at
// the end of this function, used to pop the compiler diagnostics status.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#pragma GCC diagnostic ignored "-Wreturn-type"


TaskHandle_t TsenderHandle=NULL; //handler to sender task
TaskHandle_t TreceiverHandle=NULL; //handler to reciever task
TimerHandle_t xTimer1 = NULL;
 TimerHandle_t xTimer2 = NULL;
 SemaphoreHandle_t send_Semaphore = NULL;
 SemaphoreHandle_t receive_Semaphore = NULL;
BaseType_t xTimer1Started, xTimer2Started;
QueueHandle_t Queuebus; //handler to our queue which is called Queuebus
int sent_msg=0;//counter to successfully transmited msg.
int blocked_msg=0;// counter to unsent msg due to full queue
int received_msg=0;//Counter to received msg
char msg_queue[30]; //message sent and received by queue
int sendtimervalues[6]={100,140,180,220,260,300};//array containing timer values

void queue_create(void)
{
	Queuebus=xQueueCreate(Queue_size ,sizeof(msg_queue));//create queue of messages
	if(Queuebus==NULL)//in case of memory overflow
		printf("Failed to create queue\n");

}

//Function that prints number of blocked and sent messages and reset the counter of them and of the received.
void print(){

printf("number of sent messages: %d\n",sent_msg);
printf("Number of blocked messages: %d\n",blocked_msg);
printf("number of received messages: %d\n",received_msg);
sent_msg=0;
    blocked_msg=0;
    received_msg=0;
xQueueReset( Queuebus); //clear the queue.
}

void init(){
print(); //Call the print function first to print the data required of the previous iteration and to reset the counters and queue.
static int arrayindex=0;//The index of array of the timersender value.
//In case all array values have been used.
if(arrayindex>5){
//destroy timers
xTimerDelete( xTimer1,0 );
xTimerDelete( xTimer2,0 );
printf("Game Over \n");
vTaskEndScheduler(); //Terminate the program.
}
xTimerChangePeriod( xTimer1,pdMS_TO_TICKS(sendtimervalues[arrayindex]),0); //Change the period of the timer to the new value of the array_index.
xTimerChangePeriod( xTimer2,pdMS_TO_TICKS(receiver_timevalue),0); //A trivial task as the timer value of receive is constant but it's required in project discription to be set in init.
printf("SenderTimer value is: %d\n",sendtimervalues[arrayindex]);
arrayindex++; //increment the array index for next iteration.
}

//sender task.
void Tsender (void *p1)
{
char xmessage [30];// a variable represent msg sent and received by queue.
while(1)
{if(xSemaphoreTake( send_Semaphore, 0)){
	if(received_msg!=500){ //If condition to make sure that after we received 500 messages no more messages is sent.
		if(uxQueueSpacesAvailable( Queuebus ))//if the queue not full
		{sprintf(xmessage,"Time is %d",xTaskGetTickCount());//To get the number of ticks by system.
			xQueueSend(Queuebus,(void*) xmessage, ( TickType_t ) 0);//Send msg
		sent_msg++;//Increment number of sent msg
		}
	else
		{blocked_msg++;//Increment number of blocked msg.
		}
}}}
}

void Treceiver (void *p2)
{
char recmessage [30];
while(1){if(xSemaphoreTake( receive_Semaphore, 0)){
		if(uxQueueSpacesAvailable( Queuebus )!=Queue_size)
		{
			xQueueReceive( Queuebus,(void*) recmessage,( TickType_t ) 0 );//receive msg
			received_msg++;//increment number of received msg.

		}
}}
}


int
main(int argc, char* argv[])
{
	queue_create();
	receive_Semaphore=xSemaphoreCreateBinary();//initialization of receive binarysemaphore
	send_Semaphore=xSemaphoreCreateBinary();//initialization of send binarysemaphore
	xTimer1 = xTimerCreate( "Timer1", ( pdMS_TO_TICKS(100) ), pdTRUE, ( void * ) 0, SenderTimerCallback);//creation of send timer
	xTimer2 = xTimerCreate( "Timer2", ( pdMS_TO_TICKS(receiver_timevalue) ), pdTRUE, ( void * ) 0, ReceiverTimerCallback);//creation of receive timer
	init();//calling of init function
	xTaskCreate(Tsender,"Tsender",250,(void*) 0,tskIDLE_PRIORITY,TsenderHandle); //creation of send task
	xTaskCreate(Treceiver,"Treceiver",250,(void*) 0,tskIDLE_PRIORITY,TreceiverHandle);//creation of receive task

	if( ( xTimer1 != NULL ) && ( xTimer2 != NULL ) )//check if the timers created
		{
			xTimer1Started = xTimerStart( xTimer1, 0 );//Start timer 1
			xTimer2Started = xTimerStart( xTimer2, 0 );//Start timer 2
		}

	if( xTimer1Started == pdPASS && xTimer2Started == pdPASS)
	vTaskStartScheduler();//Start RTOS schedular
	else
		printf("Timer didn't start\n");
return 0;

}

#pragma GCC diagnostic pop

// ----------------------------------------------------------------------------

//Callback function of the sender timer.
 void SenderTimerCallback( TimerHandle_t xTimer )
{
	 xSemaphoreGive( send_Semaphore ); //Give the signal to sender task by binary semaphore
}


//Callback function of the receiver timer.
 void ReceiverTimerCallback( TimerHandle_t xTimer )
{if(received_msg==500){//when received message is equal to 500 we call init function
	init();}
else{ xSemaphoreGive( receive_Semaphore );  //Give the signal to receiver task by binary semaphore

	 }}

void vApplicationMallocFailedHook( void )
{
	/* Called if a call to pvPortMalloc() fails because there is insufficient
	free memory available in the FreeRTOS heap.  pvPortMalloc() is called
	internally by FreeRTOS API functions that create tasks, queues, software
	timers, and semaphores.  The size of the FreeRTOS heap is set by the
	configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */
	for( ;; );
}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
{
	( void ) pcTaskName;
	( void ) pxTask;

	/* Run time stack overflow checking is performed if
	configconfigCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
	function is called if a stack overflow is detected. */
	for( ;; );
}
/*-----------------------------------------------------------*/

void vApplicationIdleHook( void )
{
volatile size_t xFreeStackSpace;

	/* This function is called on each cycle of the idle task.  In this case it
	does nothing useful, other than report the amout of FreeRTOS heap that
	remains unallocated. */
	xFreeStackSpace = xPortGetFreeHeapSize();

	if( xFreeStackSpace > 100 )
	{
		/* By now, the kernel has allocated everything it is going to, so
		if there is a lot of heap remaining unallocated then
		the value of configTOTAL_HEAP_SIZE in FreeRTOSConfig.h can be
		reduced accordingly. */
	}
}

void vApplicationTickHook(void) {
}

StaticTask_t xIdleTaskTCB CCM_RAM;
StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE] CCM_RAM;

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize) {
  /* Pass out a pointer to the StaticTask_t structure in which the Idle task's
  state will be stored. */
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

  /* Pass out the array that will be used as the Idle task's stack. */
  *ppxIdleTaskStackBuffer = uxIdleTaskStack;

  /* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
  Note that, as the array is necessarily of type StackType_t,
  configMINIMAL_STACK_SIZE is specified in words, not bytes. */
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

static StaticTask_t xTimerTaskTCB CCM_RAM;
static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH] CCM_RAM;

/* configUSE_STATIC_ALLOCATION and configUSE_TIMERS are both set to 1, so the
application must provide an implementation of vApplicationGetTimerTaskMemory()
to provide the memory that is used by the Timer service task. */
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize) {
  *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
  *ppxTimerTaskStackBuffer = uxTimerTaskStack;
  *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

