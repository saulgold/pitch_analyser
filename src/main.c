/**************************************************************************************************
* @file		Projects\AudioAdjuster\src\main.c
*
* Summary:
*         	main function for the pitch detection project
*
* ToDo:
*     		none
*
* Originator:
*     		Saul Goldblatt
*
* History:
* 			Version 1.00	18/03/2016	Saul Goldblatt	Initial Version copied from Audio adjuster project with modified directories
*      		Version 1.01    18/03/2016	Saul Goldblatt	FFT implemented
*      		Version 1.02    18/03/2016	Saul Goldblatt	split frequency range into thirds and uses this for bases of LED high, med, low 
*      		Version 1.03	18/03/2016	Saul Goldblatt	Uses proper peak detection method
*			Version 1.04	23/03/2016	Saul Goldblatt	replays the peak frequency with s1
*			Version 1.05 	8/04/2016	Saul Goldblatt	code cleaned and functions added
*
***************************************************************************************************/
#include <p33FJ256GP506.h>
#include <sask.h>
#include <ex_sask_generic.h>
#include <dsp.h>
#include <ADCChannelDrv.h>
#include <OCPWMDrv.h>
#include "..\inc\AT25F4096Drv.h"
#include "..\inc\G711.h"
#include "..\inc\frequency_processing.h"
#include "..\inc\modulate.h"
#include "..\inc\transform.h"
#define FRAME_SIZE 			256
#define WRITE_START_ADDRESS	0x20000		/* Flash memory address for user				*/


/*Allocate memory for input and output buffers*/
fractional		adcBuffer		[ADC_CHANNEL_DMA_BUFSIZE] 	__attribute__((space(dma)));
fractional		ocPWMBuffer		[OCPWM_DMA_BUFSIZE]		__attribute__((space(dma)));
/*Allocate memory recording*/
int 		samples			[FRAME_SIZE];
char 	encodedSamples	[FRAME_SIZE];
int 		decodedSamples	[FRAME_SIZE];
char 	flashMemoryBuffer	[AT25F4096DRV_BUFFER_SIZE];


/*variables for FFT*/
fractcomplex compx[FRAME_SIZE]__attribute__ ((space(ymemory),far));
fractcomplex compX[FRAME_SIZE]__attribute__ ((space(ymemory),far));
fractional outputSignal[FRAME_SIZE];
double compXfilteredAbs[FRAME_SIZE]__attribute__ ((space(ymemory),far));

/*variables for audio processing*/
fractional		frctAudioIn			[FRAME_SIZE]__attribute__ ((space(xmemory),far));
fractional		frctAudioOut		[FRAME_SIZE]__attribute__ ((space(xmemory),far));

/*Instantiate the drivers*/
ADCChannelHandle adcChannelHandle;
OCPWMHandle 	ocPWMHandle;
AT25F4096Handle flashMemoryHandle; 
/*Create the driver handles*/
ADCChannelHandle *pADCChannelHandle 	= &adcChannelHandle;
OCPWMHandle 	*pOCPWMHandle 		= &ocPWMHandle;
AT25F4096Handle *pFlashMemoryHandle 	= &flashMemoryHandle;

long currentReadAddress;		/* This one tracks the intro message	*/
long currentWriteAddress;		/* This one tracks the writes to flash	*/
long userPlaybackAddress;		/* This one tracks user playback		*/

int record;							/* If set means recording			*/
int playback;							/* If set means playback is in progress	*/
int erasedBeforeRecord;				/* If set means that erase before record	*/
									/* is complete	*/ 
long address;							/* Used for erasing the flash			*/


int main(void)
{
	
	/* Configure Oscillator to operate the device at 40MHz.
	 * Fosc= Fin*M/(N1*N2), Fcy=Fosc/2
	 * Fosc= 7.37M*40/(2*2)=80Mhz for 7.37M input clock */
	 
	PLLFBD=41;				/* M=39	*/
	CLKDIVbits.PLLPOST=0;		/* N1=2	*/
	CLKDIVbits.PLLPRE=0;		/* N2=2	*/
	OSCTUN=0;			
	
	__builtin_write_OSCCONH(0x01);		/*	Initiate Clock Switch to FRC with PLL*/
	__builtin_write_OSCCONL(0x01);
	while (OSCCONbits.COSC != 0b01);	/*	Wait for Clock switch to occur	*/
	while(!OSCCONbits.LOCK);

	/* Initialize flags and address variables	*/
	 record = 0;
	 playback = 0;
	 currentReadAddress = 0;
	 currentWriteAddress = WRITE_START_ADDRESS;
	 userPlaybackAddress = WRITE_START_ADDRESS;
	 address = 0;
	 erasedBeforeRecord = 0;	
	
	/*local variables*/


	
	/*initialise the board LEDs*/
	SASKInit( );

	/*Initialise Audio input and output function*/
	ADCChannelInit	(pADCChannelHandle,adcBuffer);			
	OCPWMInit		(pOCPWMHandle,ocPWMBuffer);			
	/*initialise the flash memory*/
	AT25F4096Init		(pFlashMemoryHandle,flashMemoryBuffer);	/* For the  Flash	*/
	/*start flash driver*/
	AT25F4096Start	(pFlashMemoryHandle);
	/*Start Audio input and output function*/
	ADCChannelStart	(pADCChannelHandle);
	OCPWMStart		(pOCPWMHandle);	

	/*start processing loop*/	
	while(1)
	{   
		/* Obtaing the ADC samples	*/
			while(ADCChannelIsBusy(pADCChannelHandle));
			ADCChannelRead	(pADCChannelHandle,samples,FRAME_SIZE);

			
		if( record==1){
			
			if(erasedBeforeRecord == 0){
				/* Stop the Audio input and output since this is a blocking
				 * operation. Also rewind record and playback pointers to
				 * start of the user flash area.*/
					 
				ADCChannelStop(pADCChannelHandle);
				OCPWMStop	(pOCPWMHandle);
				currentWriteAddress = WRITE_START_ADDRESS;
				userPlaybackAddress = WRITE_START_ADDRESS;
				RED_LED = SASK_LED_ON;
				YELLOW_LED = SASK_LED_OFF;
				/* Erase the user area of the flash. The intro message is not erased	*/
				for(address = WRITE_START_ADDRESS; address < AT25F4096DRV_LAST_ADDRESS; address += 0x10000){
					/* Erase each sector. Each sector is 0xFFFF long	*/
					AT25F4096IoCtl(pFlashMemoryHandle,AT25F4096DRV_WRITE_ENABLE,0);	
					AT25F4096IoCtl(pFlashMemoryHandle,AT25F4096DRV_SECTOR_ERASE,(void *)&address);	
					while(AT25F4096IsBusy(pFlashMemoryHandle));
					}
				RED_LED = 1;
				/* Since erase is complete, the next time the loop is executed
				 * dont erase the flash. Start the audio input and output	*/
				erasedBeforeRecord = 1;
				ADCChannelStart(pADCChannelHandle);
				OCPWMStart		(pOCPWMHandle);	
					
	    	}	
			else{
				/* Record the encoded audio frame. Yellow LED turns on when
				 * when recording is being performed	*/
				YELLOW_LED=0;
				G711Lin2Ulaw(samples,encodedSamples,FRAME_SIZE);
				
				while(AT25F4096IsBusy(pFlashMemoryHandle));
				AT25F4096IoCtl(pFlashMemoryHandle,AT25F4096DRV_WRITE_ENABLE,0);
				AT25F4096Write(pFlashMemoryHandle,currentWriteAddress,encodedSamples,FRAME_SIZE);
				currentWriteAddress += FRAME_SIZE;
				
				if(currentWriteAddress >= AT25F4096DRV_LAST_ADDRESS){
					YELLOW_LED = SASK_LED_OFF;
					erasedBeforeRecord = 0;
					record = 0;
				
				}
			}		
		}
		/* If playback is enabled, then start playing back samples from the
		 * user area. Playback only till the last record address and then 
		 * rewind to the start	*/
		/*Wait till the ADC has a new frame available*/
	
		/*toggle playback with s2*/

			
		if ( playback==1){
				
			GREEN_LED = SASK_LED_ON;
			erasedBeforeRecord = 0;		
			while(AT25F4096IsBusy(pFlashMemoryHandle));				
			AT25F4096Read(pFlashMemoryHandle,userPlaybackAddress,encodedSamples,FRAME_SIZE);
			while(AT25F4096IsBusy(pFlashMemoryHandle));
			userPlaybackAddress += FRAME_SIZE;
			if(userPlaybackAddress >= currentWriteAddress){
				userPlaybackAddress = WRITE_START_ADDRESS;
			}
		}
			
			/* Decode the samples	*/
			G711Ulaw2Lin (encodedSamples,decodedSamples, FRAME_SIZE);
	
			/* Wait till the OC is available for a new  frame	*/
			while(OCPWMIsBusy(pOCPWMHandle));	
		
			/* Write the frame to the output	*/
			OCPWMWrite (pOCPWMHandle,decodedSamples,FRAME_SIZE);
			
				
			/* The CheckSwitch functions are defined in sask.c	*/

			if((CheckSwitchS1()) == 1)
			{
				/* Toggle the record function and Yellow led.
				 * Rewind the intro message playback pointer. 
				 * And if recording, disable playback.*/
				 
				
				record = 1;				
				currentReadAddress = 0;	
				erasedBeforeRecord = 0;
				if(record == 1)
				{
					playback = 0;
					GREEN_LED = SASK_LED_OFF;
				}
				else
				{
					YELLOW_LED = SASK_LED_OFF;
				}
			}
			
			if((CheckSwitchS2()) == 1)
			{
				/* Toggle the record function and AMBER led.
				 * Rewind the intro message playback pointer. 
				 * And if recording, disable playback.*/
				 
				GREEN_LED =SASK_LED_OFF;
				playback =1;
				currentReadAddress = 0;	
				userPlaybackAddress = WRITE_START_ADDRESS;	
				if(playback == 1)
				{
					record = 0;
					YELLOW_LED = SASK_LED_OFF;
				}
			}
			
	
}
}
