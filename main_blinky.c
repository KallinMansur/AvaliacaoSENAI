/* Standard includes. */
#include <stdio.h>
#include <conio.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "semphr.h"

/* Priorities at which the tasks are created. */
#define mainQUEUE_RECEIVE_TASK_PRIORITY		( tskIDLE_PRIORITY + 2 )
#define	mainQUEUE_SEND_TASK_PRIORITY		( tskIDLE_PRIORITY + 1 )

/* The rate at which data is sent to the queue.  The times are converted from
milliseconds to ticks using the pdMS_TO_TICKS() macro. */
#define mainTASK_SEND_FREQUENCY_MS			pdMS_TO_TICKS( 200UL )
#define mainTIMER_SEND_FREQUENCY_MS			pdMS_TO_TICKS( 2000UL )

/* The number of items the queue can hold at once. */
#define mainQUEUE_LENGTH					( 5 )

/* The values sent to the queue receive task from the queue send task and the
queue send software timer respectively. */
#define mainVALUE_SENT_FROM_TASK			( 100UL )
#define mainVALUE_SENT_FROM_TIMER			( 200UL )


/* This demo allows for users to perform actions with the keyboard. */
#define mainNO_KEY_PRESS_VALUE              ( -1 )
#define mainRESET_TIMER_KEY                 ( 'r' )

/*-----------------------------------------------------------*/

/*
 * The tasks as described in the comments at the top of this file.
 */
static void prvQueueReceiveTask( void *pvParameters );
static void prvQueueSendTask( void *pvParameters );

/*
 * The callback function executed when the software timer expires.
 */
static void prvQueueSendTimerCallback( TimerHandle_t xTimerHandle );

/*-----------------------------------------------------------*/

/* The queue used by both tasks. */
static QueueHandle_t xQueue = NULL;

/* A software timer that is started from the tick hook. */
static TimerHandle_t xTimer = NULL;

/*-----------------------------------------------------------*/

/*** INICIO DAS MODIFICAÇÕES ***/

#define ADC_Renew_FREQUENCY_MS			pdMS_TO_TICKS( 1UL )
#define Consumo_FREQUENCY_MS			pdMS_TO_TICKS( 3000UL )
#define mainVALUE_SENT_FROM_ADC			( 300UL )
#define mainVALUE_SENT_FROM_CONSUMO	    ( 400UL )
#define OBTER_KEY                 ( 'o' )
#define ZERAR_KEY                 ( 'z' )
#define OUTPUT_FILE_NAME                   "datadump.csv"


static TimerHandle_t ADCTimer = NULL;
static void prvADCcallback(TimerHandle_t xTimerHandle);
static void prvSaveDATADUMPFile(void);

static TimerHandle_t CONSUMOTimer = NULL;
static void prvCONSUMOcallback(TimerHandle_t xTimerHandle);

const int Canal_Ref[] = { 0, 121, 224, 296, 327, 312, 252, 158, 41, -81, -193, -277, -322, -322, -277, -193, -81, 41, 158, 252, 312,
327, 296, 224, 121 };

const int Canal_Sen[] = { 0, 145, 269, 356, 392, 374, 303, 189, 49, -98, -231, -332, -386, -386, -332, -231, -98, 49, 189, 303, 374,
392, 356, 269, 145 };

char RINGBUFFER_POSICAO_ADC=0;

int buffer_adc_ref[25];
int buffer_adc_sensor[25];
char RINGBUFFER_POSICAO_LEITURA=0;
char RINGBUFFER_POSICAO_GRAVACAO = 0;
int buffer_result_int[3] = { 0,0,0 };
float buffer_result[3];



/*** SEE THE COMMENTS AT THE TOP OF THIS FILE ***/
void main_blinky( void )
{
const TickType_t xTimerPeriod = mainTIMER_SEND_FREQUENCY_MS;

const TickType_t ADCTimerPeriod = ADC_Renew_FREQUENCY_MS;
const TickType_t CONSUMOTimerPeriod = Consumo_FREQUENCY_MS;

    printf( "\r\nStarting the blinky demo. Press \'%c\' to reset the software timer used in this demo.\r\n\r\n", mainRESET_TIMER_KEY );

	/* Create the queue. */
	xQueue = xQueueCreate( mainQUEUE_LENGTH, sizeof( uint32_t ) );

	if( xQueue != NULL )
	{
		/* Start the two tasks as described in the comments at the top of this
		file. */
		xTaskCreate( prvQueueReceiveTask,			/* The function that implements the task. */
					"Rx", 							/* The text name assigned to the task - for debug only as it is not used by the kernel. */
					configMINIMAL_STACK_SIZE, 		/* The size of the stack to allocate to the task. */
					NULL, 							/* The parameter passed to the task - not used in this simple case. */
					mainQUEUE_RECEIVE_TASK_PRIORITY,/* The priority assigned to the task. */
					NULL );							/* The task handle is not required, so NULL is passed. */

		xTaskCreate( prvQueueSendTask, "TX", configMINIMAL_STACK_SIZE, NULL, mainQUEUE_SEND_TASK_PRIORITY, NULL );

        ADCTimer = xTimerCreate("Atualiza ADC",				/* The text name assigned to the software timer - for debug only as it is not used by the kernel. */
            ADCTimerPeriod,		/* The period of the software timer in ticks. */
            pdTRUE,			    /* xAutoReload is set to pdTRUE, so this timer goes off periodically with a period of xTimerPeriod ticks. */
            2,				/* The timer's ID is not used. */
            prvADCcallback);/* The function executed when the timer expires. */
        xTimerStart(ADCTimer, 0); /* The scheduler has not started so use a block time of 0. */

        CONSUMOTimer = xTimerCreate("Atualiza Consumo",				/* The text name assigned to the software timer - for debug only as it is not used by the kernel. */
            CONSUMOTimerPeriod,		/* The period of the software timer in ticks. */
            pdTRUE,			    /* xAutoReload is set to pdTRUE, so this timer goes off periodically with a period of xTimerPeriod ticks. */
            3,				/* The timer's ID is not used. */
            prvCONSUMOcallback);/* The function executed when the timer expires. */
        xTimerStart(CONSUMOTimer, 0); /* The scheduler has not started so use a block time of 0. */

		/* Start the tasks and timer running. */
		vTaskStartScheduler();
	}

	/* If all is well, the scheduler will now be running, and the following
	line will never be reached.  If the following line does execute, then
	there was insufficient FreeRTOS heap memory available for the idle and/or
	timer tasks	to be created.  See the memory management section on the
	FreeRTOS web site for more details. */
	for( ;; );
}
/*-----------------------------------------------------------*/

static void prvADCcallback(TimerHandle_t ADCTimerHandle)
{
    const uint32_t ulValueToSend = mainVALUE_SENT_FROM_ADC;
    int i;

    //Lê a próxima posição do buffer, incrementa a posição do ringbuffer na segunda instrução
    buffer_adc_ref[RINGBUFFER_POSICAO_GRAVACAO] = Canal_Ref[RINGBUFFER_POSICAO_ADC];
    buffer_adc_sensor[RINGBUFFER_POSICAO_GRAVACAO++] = Canal_Sen[RINGBUFFER_POSICAO_ADC++];

    //Se chegar ao fim do ringbuffer, volta ao inicio
    if (RINGBUFFER_POSICAO_GRAVACAO >= 25) RINGBUFFER_POSICAO_GRAVACAO = 0;
    if (RINGBUFFER_POSICAO_ADC >= 25) RINGBUFFER_POSICAO_ADC = 0;

    // Compara todos as posições do buffer de leitura e dentifica o maior valor
    for (i = 0; i < 25; i++) {
        if (buffer_result_int[0] < buffer_adc_ref[i]) buffer_result_int[0] = buffer_adc_ref[i];
        if (buffer_result_int[1] < buffer_adc_sensor[i]) buffer_result_int[1] = buffer_adc_sensor[i];
    }

    //Conversão de um sinal 16 bits para volts, considera-se 2^15 qundo há valores negativos
    buffer_result[0] = (float)buffer_result_int[0] * 1 / 32768;
    buffer_result[1] = (float)buffer_result_int[1] * 1 / 32768;

    buffer_result[2] = buffer_result[0] - buffer_result[1];

    /* This is the software timer callback function.  The software timer has a
    period of two seconds and is reset each time a key is pressed.  This
    callback function will execute if the timer expires, which will only happen
    if a key is not pressed for two seconds. */

    /* Avoid compiler warnings resulting from the unused parameter. */
    (void)ADCTimerHandle;

    /* Send to the queue - causing the queue receive task to unblock and
    write out a message.  This function is called from the timer/daemon task, so
    must not block.  Hence the block time is set to 0. */
    xQueueSend(xQueue, &ulValueToSend, 0U);
}

static void prvCONSUMOcallback(TimerHandle_t CONSUMOTimerHandle)
{
    const uint32_t ulValueToSend = mainVALUE_SENT_FROM_CONSUMO;

    /* Avoid compiler warnings resulting from the unused parameter. */
    (void)CONSUMOTimerHandle;

    /* Send to the queue - causing the queue receive task to unblock and
    write out a message.  This function is called from the timer/daemon task, so
    must not block.  Hence the block time is set to 0. */
    xQueueSend(xQueue, &ulValueToSend, 0U);
}
/*-----------------------------------------------------------*/

static void prvQueueSendTask( void *pvParameters )
{
TickType_t xNextWakeTime;
const TickType_t xBlockTime = mainTASK_SEND_FREQUENCY_MS;
const uint32_t ulValueToSend = mainVALUE_SENT_FROM_TASK;

	/* Prevent the compiler warning about the unused parameter. */
	( void ) pvParameters;

	/* Initialise xNextWakeTime - this only needs to be done once. */
	xNextWakeTime = xTaskGetTickCount();

	for( ;; )
	{
		/* Place this task in the blocked state until it is time to run again.
		The block time is specified in ticks, pdMS_TO_TICKS() was used to
		convert a time specified in milliseconds into a time specified in ticks.
		While in the Blocked state this task will not consume any CPU time. */
		vTaskDelayUntil( &xNextWakeTime, xBlockTime );

		/* Send to the queue - causing the queue receive task to unblock and
		write to the console.  0 is used as the block time so the send operation
		will not block - it shouldn't need to block as the queue should always
		have at least one space at this point in the code. */
		xQueueSend( xQueue, &ulValueToSend, 0U );
	}
}

static void prvSaveDATADUMPFile(void)
{
    FILE* OutputFile;

    OutputFile = fopen(OUTPUT_FILE_NAME, "a");

    if (OutputFile != NULL)
    {
        fprintf(OutputFile, "%f, %f, %f \n", buffer_result[0], buffer_result[1], buffer_result[2]);
        fclose(OutputFile);
    }
    else
    {
    }
}

/*-----------------------------------------------------------*/



static void prvQueueReceiveTask( void *pvParameters )
{
uint32_t ulReceivedValue;
char bufferConsumo[100];

	/* Prevent the compiler warning about the unused parameter. */
	( void ) pvParameters;

	for( ;; )
	{
		/* Wait until something arrives in the queue - this task will block
		indefinitely provided INCLUDE_vTaskSuspend is set to 1 in
		FreeRTOSConfig.h.  It will not use any CPU time while it is in the
		Blocked state. */
		xQueueReceive( xQueue, &ulReceivedValue, portMAX_DELAY );

        /* Enter critical section to use printf. Not doing this could potentially cause
           a deadlock if the FreeRTOS simulator switches contexts and another task
           tries to call printf - it should be noted that use of printf within
           the FreeRTOS simulator is unsafe, but used here for simplicity. */
        taskENTER_CRITICAL();
        {
            /*  To get here something must have been received from the queue, but
            is it an expected value?  Normally calling printf() from a task is not
            a good idea.  Here there is lots of stack space and only one task is
            using console IO so it is ok.  However, note the comments at the top of
            this file about the risks of making Windows system calls (such as
            console output) from a FreeRTOS task. */
            if (ulReceivedValue == mainVALUE_SENT_FROM_TASK)
            {
                //printf("Message received from task - idle time %llu%%\r\n", ulTaskGetIdleRunTimePercent());
            }
            else if (ulReceivedValue == mainVALUE_SENT_FROM_TIMER)
            {
                //printf("Message received from software timer\r\n");
            }
            else if (ulReceivedValue == mainVALUE_SENT_FROM_ADC)
            {
            }
            else if (ulReceivedValue == mainVALUE_SENT_FROM_CONSUMO)
            {
                vTaskGetRunTimeStats(bufferConsumo);
                printf("%s", bufferConsumo);
            }
            else
            {
                printf("Unexpected message\r\n");
            }
        }
        taskEXIT_CRITICAL();
	}
}
/*-----------------------------------------------------------*/

/* Called from prvKeyboardInterruptSimulatorTask(), which is defined in main.c. */
void vBlinkyKeyboardInterruptHandler( int xKeyPressed )
{
    /* Handle keyboard input. */
    switch ( xKeyPressed )
    {
    case OBTER_KEY:

        printf("%f, %f, %f \n", buffer_result[0], buffer_result[1], buffer_result[2]);
        prvSaveDATADUMPFile();
        break; 
    case ZERAR_KEY:
        if (remove(OUTPUT_FILE_NAME) == 0) printf("Arquivo deletado\n");;
        break;

    default:
        break;
    }
}


