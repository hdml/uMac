/*
 * uMac.c
 *
 *  Created on: Nov 1, 2012
 */


//Librerias
#include "McuInit.h"                /*CPU and System Clock related functions*/
#include "EmbeddedTypes.h"          /*Include special data types*/       
#include "SMAC_Interface.h"         /*Include all OTA functionality*/
#include "uMac.h"        			/*Include all OTA functionality*/
#include "app_config.h"
#include "OTAP_Interface.h"
#include "PLM_config.h"
#include "Timer_Interface.h"		/*Include all the Timer functions*/


#define MyID 1

//Definiciones
typedef enum{
	uMac_NoInit = 0,
	uMac_Init = 1,
	uMac_WaitRx = 2,
	uMac_Rx = 3,
	uMac_Tx = 4
} uMac_Engine_State;

channels_t       bestChannel;
bool_t           bScanDone;

static uMac_Engine_State uMac_Current_State;
static bool_t uMac_On = FALSE;
//static uint8_t uMac_Best_Channel;

//Variables
static uint8_t gau8RxDataBuffer[130]; /* 123 bytes is the SDU max size in non
                                         promiscuous mode. 
                                         125 bytes is the SDU max size in 
                                         promiscuous mode. 
                                         You have to consider the SDU plus 5 more 
                                         bytes for SMAC header and that the buffer 
                                         can not be bigger than 130 */
static uint8_t gau8TxDataBuffer[128]; /* This buffer can be as big as the biggest
                                         packet to transmit in the app plus 3 
                                         bytes reserved for SMAC packet header.
                                         The buffer can not be bigger than 128 */
        
static	txPacket_t * AppTxPacket;
static	rxPacket_t * AppRxPacket;

static	bool_t bTimerFlag;
static	tmrChannelConfig_t timerConfig;


static uMac_Packet * uMac_RxPacket;
static uMac_Packet * uMac_TxPacket;

uint16_t	ChannelsToScan = 0xFFF;
uint8_t		ChannelsEnergy[16];

bool_t			bTxDone;
bool_t			bRxDone;
bool_t			bDoTx;
bool_t			bToggle = TRUE;

uMac_nodeType uMactype;
uint8_t uMacbroad = 254, i = 0;


channels_t Channels[] = {gChannel11_c, gChannel12_c, gChannel13_c, gChannel14_c, gChannel15_c,
			gChannel16_c, gChannel17_c, gChannel18_c, gChannel19_c, gChannel20_c, gChannel21_c,
			gChannel22_c, gChannel23_c, gChannel24_c, gChannel25_c, gChannel26_c};

//Prototipos
/* Place it in NON_BANKED memory */
#ifdef MEMORY_MODEL_BANKED
#pragma CODE_SEG __NEAR_SEG NON_BANKED
#else
#pragma CODE_SEG DEFAULT
#endif /* MEMORY_MODEL_BANKED */

void TimerCallBack(void);

#pragma CODE_SEG DEFAULT

void InitSmac(void);
void uMac_Txf(void);
void SetTimer(uint16_t waitTimeMs);

void Init_uMac(uMac_nodeType type /*, uint8_t dest, uMac_txCallBack TxCallBack, uMac_rxCallBack RxCallBack*/) {
	uMac_On = TRUE;
	//uMactype = type;
	//uMacdest = dest;
	
	InitSmac();
	uMac_RxPacket = (uMac_Packet *)AppRxPacket->smacPdu.u8Data;
	uMac_TxPacket = (uMac_Packet *)AppTxPacket->smacPdu.u8Data;
	
	//Set the timer
	(void)Tmr_Init();
	SetTimer(62500);
	
	uMac_Current_State = uMac_NoInit;
	//guardar tipo de nodo y callbacks
}

void uMac_Txf() {
	bDoTx = TRUE;
}

void uMac_Engine(){	

	switch (uMac_Current_State) {
		case uMac_NoInit:
			if(uMac_On==TRUE){
				uMac_On = FALSE;
				(void) MLMEScanRequest(ChannelsToScan, gScanModeED_c, ChannelsEnergy);
				uMac_Current_State = uMac_Init;
			}
			break;
		case uMac_Init:
			if(bScanDone==TRUE){
				bScanDone = FALSE;
				//(void) MLMESetChannelRequest(bestChannel);
				(void) MLMESetChannelRequest(Channels[0]);
				(void) MLMERXEnableRequest(AppRxPacket,0); 
				uMac_Current_State = uMac_WaitRx;
			}
		case uMac_WaitRx:
				if(bRxDone == TRUE) {
					bRxDone = FALSE;
					if (AppRxPacket->rxStatus == rxSuccessStatus_c) {
						if (uMac_RxPacket->Pan_ID == 10) {
							if (uMac_RxPacket->Packet_Type == 0) {
								//Si el paquete es de control, responder con otro hello
								uMac_TxPacket->Dest_Add = uMac_RxPacket->Source_Add;
								uMac_TxPacket->Packet_Type = 0;
								uMac_TxPacket->Pan_ID = 10;
								uMac_TxPacket->Source_Add = 1;
								//AppTxPacket->u8DataLength = 4;
								(void) MCPSDataRequest(AppTxPacket);
							} else {
								// Llamar a la callback
							}
							uMac_Current_State = uMac_Tx;
							(void) MLMERXEnableRequest(AppRxPacket, 0);
						} else {
							uMac_Current_State = uMac_NoInit;
							uMac_On = TRUE;
						}
					} else if (AppRxPacket->rxStatus == rxTimeOutStatus_c) {
						uMac_Current_State = uMac_NoInit;
						uMac_On = TRUE;
					}
				}
				if (bDoTx == TRUE) {
					bDoTx = FALSE;
					// Deshabilitar la recepcion antes de transmitir (necesario por SMAC)
					(void) MLMERXDisableRequest();
					(void) MCPSDataRequest(AppTxPacket);
					uMac_Current_State = uMac_Tx;
				}
				
				break;
		case uMac_Tx:
			if(bTxDone == TRUE){
				bTxDone = FALSE;
				uMac_Current_State = uMac_Init;
				(void) MLMERXEnableRequest(AppRxPacket, 0);
			}
			break;
	}
}

/************************************************************************************
*
* SetTimer
*
************************************************************************************/

void SetTimer(uint16_t WaitTime){
	timerConfig.tmrChannel = gTmrChannel0_c;
	timerConfig.tmrChannOptMode = gTmrOutputCompare_c;
	timerConfig.tmrCompareVal = WaitTime; 
	timerConfig.tmrPinConfig.tmrOutCompState = gTmrPinNotUsedForOutComp_c;
	
	(void)Tmr_SetCallbackFunc(gTmr3_c, gTmrChannel0Event_c, TimerCallBack);
	(void)Tmr_SetClkConfig(gTmr3_c, gTmrBusRateClk_c, gTmrClkDivBy128_c); //62500 cuentas por segundo
	(void)Tmr_SetChannelConfig(gTmr3_c, &timerConfig);
	(void)Tmr_Enable(gTmr3_c,gTmrClkDivBy128_c,WaitTime);
}

/************************************************************************************
*
* InitSmac
*
************************************************************************************/
void InitSmac(void)
{
    AppTxPacket = (txPacket_t*)gau8TxDataBuffer;
    AppRxPacket = (rxPacket_t*)gau8RxDataBuffer; 
    AppRxPacket->u8MaxDataLength = gMaxSmacSDULenght_c;
    // AppTxPacket->smacPdu.u8Data[0] = 'T';
   /* */
    AppTxPacket->u8DataLength = 10;
    
    
    (void)MLMERadioInit();
    (void)MLMESetClockRate(gClko16MHz_c);
    (void)MCU_UseExternalClock();
    
    (void)MLMESetTmrPrescale(gTimeBase250kHz_c);
    while (gErrorNoError_c != MLMESetChannelRequest(gDefaultChannelNumber_c));
    (void)MLMEPAOutputAdjust(gDefaultOutputPower_c);
    (void)MLMEFEGainAdjust(gGainOffset_c);
 }

/* Place it in NON_BANKED memory */
#ifdef MEMORY_MODEL_BANKED
#pragma CODE_SEG __NEAR_SEG NON_BANKED
#else
#pragma CODE_SEG DEFAULT
#endif /* MEMORY_MODEL_BANKED */

/************************************************************************************
* TimerCallBack
* 
*
*
************************************************************************************/

void TimerCallBack(void)
{
	bTimerFlag = TRUE;
}

/************************************************************************************
* MLMEScanComfirm
* 
*
*
************************************************************************************/
void MLMEScanComfirm(channels_t ClearestChann)
{
  bestChannel = ClearestChann; 
  bScanDone = TRUE;
}

/************************************************************************************
* MCPSDataIndication
* 
*
*
************************************************************************************/
void MCPSDataIndication(rxPacket_t *gsRxPacket)
{  
  //Otap_OpcMCPSDataIndication(gsRxPacket);
  bRxDone = TRUE;
}

/************************************************************************************
* MCPSDataComfirm
* 
*
*
************************************************************************************/
void MCPSDataComfirm(txStatus_t TransmissionResult)
{  
    //Otap_OpcMCPSDataComfirm(&TransmissionResult);
    bTxDone = TRUE;
}

void MLMEResetIndication(void)
{
  
}


void MLMEWakeComfirm(void)
{
  
}


#pragma CODE_SEG DEFAULT