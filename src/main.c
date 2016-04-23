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
#include <libpic30.h>
#include "..\inc\AT25F4096Drv.h"
#include "..\inc\G711.h"
#include "..\inc\frequency_processing.h"
#include "..\inc\modulate.h"
#include "..\inc\transform.h"
#define FRAME_SIZE 			128
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
int fileSize=0;									/* is complete	*/ 
long address;							/* Used for erasing the flash			*/
int storage [10][128];
int i1 __attribute__((space(dma)));
int j __attribute__((space(dma))); 
int k __attribute__((space(dma)));
int main(void)
{
	j=0;
	k=0;
		float clock_frequency, cycle_time, delay_time;
	unsigned long delay_cycles;
	/* Configure Oscillator to operate the device at 40MHz.
	 * Fosc= Fin*M/(N1*N2), Fcy=Fosc/2
	 * Fosc= 7.37M*40/(2*2)=80Mhz for 7.37M input clock */
	 
	PLLFBD=41;				/* M=39	*/
	CLKDIVbits.PLLPOST=0;		/* N1=2	*/
	CLKDIVbits.PLLPRE=0;		/* N2=2	*/
	OSCTUN=0;	
			
		clock_frequency = 40e6;
	cycle_time = 1 / clock_frequency;
	delay_time = 0.02;
	delay_cycles = delay_time / cycle_time;
	
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

			i1=0;
		if( record==1){
			k=0;

			
				//G711Lin2Ulaw(samples,encodedSamples,FRAME_SIZE);
				for(i1=0;i1<FRAME_SIZE;i1=i1+1){
					storage[j][i1] = samples[i1];
					
				}
				j++;			
							
		}
		/* If playback is enabled, then start playing back samples from the
		 * user area. Playback only till the last record address and then 
		 * rewind to the start	*/
		/*Wait till the ADC has a new frame available*/
	
		/*toggle playback with s2*/

			
		if ( playback==1){
			j=0;
				

			for(i1=0;i1<FRAME_SIZE;i1++){
					samples[i1] = storage[k][i1];
				
				}
				
				k++;
		}
				while(OCPWMIsBusy(pOCPWMHandle));	
			OCPWMWrite (pOCPWMHandle,samples,FRAME_SIZE);
			/* Decode the samples	*/
			//G711Ulaw2Lin (encodedSamples,decodedSamples, FRAME_SIZE);
	    //	__delay32( delay_cycles );
			/* Wait till the OC is available for a new  frame	*/
		
			
				
			/* The CheckSwitch functions are defined in sask.c	*/

			if(SWITCH_S1== 0)
			{
					 
				
				record = 1;		
				playback=0;
						
				GREEN_LED =0;
				YELLOW_LED=1;
			}
			
			if(SWITCH_S2 == 0)
			{
				/* Toggle the record function and AMBER led.
				 * Rewind the intro message playback pointer. 
				 * And if recording, disable playback.*/
				playback=1; 
				GREEN_LED =1;
				YELLOW_LED=0;
				record = 0;
			}
			if(SWITCH_S1==1 && SWITCH_S2==1){
				playback =0;
				record=0;
				GREEN_LED =1;
				YELLOW_LED=1;				
			}
	
}
}


void WriteToSerialFlash(AT25F4096Handle * pHandle, unsigned long address, char * source, int length)
{
	/* This function will write length number of bytes from 
	 * source to serial flash memory location address*/
	 
	 int acceptedBytes = 0;
	
	while(length != 0 )
	{
		while(AT25F4096IsBusy(pHandle));
		AT25F4096IoCtl(pHandle,AT25F4096DRV_WRITE_ENABLE,0);	
		acceptedBytes = AT25F4096Write(pHandle,address,(source+acceptedBytes),length);
		length -= acceptedBytes;
		address += acceptedBytes;
		while(AT25F4096IsBusy(pHandle));
	}
}

int VerifyWriteToSerialFlash(AT25F4096Handle * pHandle, unsigned long address, char * verifyWithData, int length)	
{
	/* This function will compare the serial flash data stored at
	 * address and verify it with data in verifyWithData. It will 
	 * return a 1 if comparision passes, 0 otherwise */
	 
	int 		verifyIndex;
	char	verifyFlashRead;
	
	for(verifyIndex = 0; verifyIndex < length;verifyIndex++,address++)
	{
		AT25F4096Read(pHandle,address,&verifyFlashRead,1);
		while(AT25F4096IsBusy(pHandle));
		if(verifyFlashRead !=verifyWithData[verifyIndex])
		{
			return(0);
		}
	}
	return(1);
}
