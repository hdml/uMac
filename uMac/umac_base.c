#include "McuInit.h"
#include "EmbeddedTypes.h" 
#include "SMAC_Interface.h"
#include "uMac.h"
#include "app_config.h"
#include "OTAP_Interface.h"
#include "PLM_config.h"
#include "Timer_Interface.h"

#define MyID 0
#define NOISE_ENERGY 190
#define MAX_SEND_ATTEMPTS 5

typedef enum {
    uMac_NoInit,
    uMac_Init,
    uMac_WaitRx,
    uMac_Rx,
    uMac_Tx,
    uMac_Timer
} uMac_Engine_State;

static uMac_Engine_State uMac_Current_State;
static bool_t uMac_On = FALSE;

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
        
static txPacket_t *AppTxPacket;
static rxPacket_t *AppRxPacket;

static uMac_Packet *uMac_RxPacket;
static uMac_Packet *uMac_TxPacket;

static channels_t bestChannel;

bool_t bTxDone, bRxDone, bScanDone, bDoTx, bTimerFlag = TRUE;
uint8_t uMacbroad = 254, ui = 0, send_attempts = 0, ChannelsEnergy[16];
uint16_t ChannelsToScan = 0xFFF;

tmrChannelConfig_t timerConfig;

void InitSmac (void);
void uMac_Txf (void);
void SetTimer (uint16_t);

#ifdef MEMORY_MODEL_BANKED
#pragma CODE_SEG __NEAR_SEG NON_BANKED
#else
#pragma CODE_SEG DEFAULT
#endif /* MEMORY_MODEL_BANKED */

void TimerCallBack (void);

#pragma CODE_SEG DEFAULT

void
Init_uMac (void)
{
    InitSmac();

    uMac_On = TRUE;
    (void) Tmr_Init();
    uMac_RxPacket = (uMac_Packet *)AppRxPacket->smacPdu.u8Data;
    uMac_TxPacket = (uMac_Packet *)AppTxPacket->smacPdu.u8Data;
    uMac_Current_State = uMac_NoInit;
}

void
SetTimer (uint16_t timerCount)
{   
    timerConfig.tmrChannel = gTmrChannel0_c;
    timerConfig.tmrChannOptMode = gTmrOutputCompare_c;
    timerConfig.tmrCompareVal = timerCount; 
    timerConfig.tmrPinConfig.tmrOutCompState = gTmrPinNotUsedForOutComp_c;

    (void) Tmr_SetCallbackFunc(gTmr3_c, gTmrOverEvent_c, TimerCallBack);
    (void) Tmr_SetClkConfig(gTmr3_c, gTmrBusRateClk_c, gTmrClkDivBy128_c); //62500 cuentas por segundo
    (void) Tmr_SetChannelConfig(gTmr3_c, &timerConfig);
    (void) Tmr_Enable(gTmr3_c,gTmrClkDivBy128_c, timerCount);
}

void
uMac_Txf (void)
{
    bDoTx = TRUE;
}

void
uMac_Engine (void)
{   
    switch (uMac_Current_State) {
    case uMac_NoInit:
        if (uMac_On == TRUE) {
            uMac_On = FALSE;
            (void) MLMEScanRequest(ChannelsToScan, gScanModeED_c, ChannelsEnergy);
            uMac_Current_State = uMac_Init;
        }
        break;
    case uMac_Init:
        if (bScanDone == TRUE) {
            bScanDone = FALSE;
            (void) MLMESetChannelRequest(bestChannel);
            (void) MLMERXEnableRequest(AppRxPacket, 0); 
            uMac_Current_State = uMac_WaitRx;
        }
        break;
    case uMac_WaitRx:
        if (bRxDone == TRUE) {
            if (AppRxPacket->rxStatus == rxSuccessStatus_c) {
                if (uMac_RxPacket->Pan_ID == 10) {
                    if (uMac_RxPacket->Dest_Add == uMacbroad && uMac_RxPacket->Packet_Type == 0) {
                        if (bTimerFlag == TRUE) {
                            bTimerFlag = FALSE;
                            if (send_attempts > MAX_SEND_ATTEMPTS) {
                                bRxDone = FALSE;
                                send_attempts = 0;
                                (void) MLMERXEnableRequest(AppRxPacket, 0);
                            }

                            if (MLMEEnergyDetect() > NOISE_ENERGY) {
                                bRxDone = FALSE;
                                send_attempts = 0;
                                uMac_TxPacket->Dest_Add = uMacbroad;
                                uMac_TxPacket->Packet_Type = 0;
                                uMac_TxPacket->Pan_ID = 10;
                                uMac_TxPacket->Source_Add = MyID;
                                (void) MCPSDataRequest(AppTxPacket);
                                uMac_Current_State = uMac_Tx;
                            } else {
                                send_attempts++;
                                (void) SetTimer(10000 * MyID);
                            }
                        }
                    } else if (uMac_RxPacket->Dest_Add == MyID) {
                        // Llamar callback
                        bRxDone = FALSE;
                    }
                }
            }
        }

        if (bDoTx == TRUE) {
            (void) MLMERXDisableRequest();  // Deshabilitar la recepcion antes de transmitir (necesario por SMAC)

            if (send_attempts > MAX_SEND_ATTEMPTS) {
                bDoTx = FALSE;
                send_attempts = 0;
                (void) MLMERXEnableRequest(AppRxPacket, 0);
            }

            if (MLMEEnergyDetect() > NOISE_ENERGY) {    
                bDoTx = FALSE;
                (void) MCPSDataRequest(AppTxPacket);
                uMac_Current_State = uMac_Tx;
            } else {
                send_attempts++;
                (void) SetTimer(MyID*);
                uMac_Current_State = uMac_Timer;
            }
        }
        
        break;
    case uMac_Timer:
        if (bTimerFlag2 == TRUE) {
            uMac_Current_State = uMac_WaitRx;
        }
        break;
    case uMac_Tx:
        if (bTxDone == TRUE) {
            bTxDone = FALSE;
            uMac_Current_State = uMac_WaitRx;
            (void) MLMERXEnableRequest(AppRxPacket, 0);
        }
        break;
    }
}

void
InitSmac (void)
{
    AppTxPacket = (txPacket_t *) gau8TxDataBuffer;
    AppTxPacket->u8DataLength = 10;

    AppRxPacket = (rxPacket_t *) gau8RxDataBuffer; 
    AppRxPacket->u8MaxDataLength = gMaxSmacSDULenght_c;
    
    
    (void)MLMERadioInit();
    (void)MLMESetClockRate(gClko16MHz_c);
    (void)MCU_UseExternalClock();
    
    (void)MLMESetTmrPrescale(gTimeBase250kHz_c);
    while (gErrorNoError_c != MLMESetChannelRequest(gDefaultChannelNumber_c));
    (void)MLMEPAOutputAdjust(gDefaultOutputPower_c);
    (void)MLMEFEGainAdjust(gGainOffset_c);
 }

#ifdef MEMORY_MODEL_BANKED
#pragma CODE_SEG __NEAR_SEG NON_BANKED
#else
#pragma CODE_SEG DEFAULT
#endif /* MEMORY_MODEL_BANKED */

void
MLMEScanComfirm (channels_t ClearestChann)
{
    bestChannel = ClearestChann; 
    bScanDone = TRUE;
}

void
MCPSDataIndication (rxPacket_t *gsRxPacket)
{  
    bRxDone = TRUE;
}

void
MCPSDataComfirm (txStatus_t TransmissionResult)
{  
    bTxDone = TRUE;
}

void
TimerCallBack (void)
{
    bTimerFlag = TRUE;
    (void) Tmr_Disable(gTmr3_c); 
}

void
MLMEResetIndication (void)
{

}

void
MLMEWakeComfirm (void)
{

}

#pragma CODE_SEG DEFAULT