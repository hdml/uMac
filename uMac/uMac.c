#include "McuInit.h"
#include "EmbeddedTypes.h"       
#include "SMAC_Interface.h"
#include "uMac.h"
#include "app_config.h"
#include "OTAP_Interface.h"
#include "PLM_config.h"
#include "Timer_Interface.h"

#define MyID 1
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

bool_t bTxDone, bRxDone, bDoTx, bTimerFlag = FALSE;
uint8_t uMacbroad = 254, ui = 0, send_attempts = 0;
channels_t bestChannel, Channels[] = {gChannel11_c, gChannel12_c, gChannel13_c, gChannel14_c, gChannel15_c,
            gChannel16_c, gChannel17_c, gChannel18_c, gChannel19_c, gChannel20_c, gChannel21_c,
            gChannel22_c, gChannel23_c, gChannel24_c, gChannel25_c, gChannel26_c};

tmrChannelConfig_t timerConfig;

void InitSmac (void);
void uMac_Txf (void);
void SetTimer (uint16_t, uint8_t);

#ifdef MEMORY_MODEL_BANKED
#pragma CODE_SEG __NEAR_SEG NON_BANKED
#else
#pragma CODE_SEG DEFAULT
#endif /* MEMORY_MODEL_BANKED */

void TimerCallBack1 (void);
void TimerCallBack2 (void);

#pragma CODE_SEG DEFAULT

void
Init_uMac (void)
{
    InitSmac();

    uMac_On = TRUE;
    uMac_RxPacket = (uMac_Packet *)AppRxPacket->smacPdu.u8Data;
    uMac_TxPacket = (uMac_Packet *)AppTxPacket->smacPdu.u8Data;
    uMac_Current_State = uMac_NoInit;
}

void
uMac_Txf (void)
{
    bDoTx = TRUE;
}

void
SetTimer (uint16_t timerCount, uint8_t t)
{   
    timerConfig.tmrChannel = gTmrChannel0_c;
    timerConfig.tmrChannOptMode = gTmrOutputCompare_c;
    timerConfig.tmrCompareVal = timerCount; 
    timerConfig.tmrPinConfig.tmrOutCompState = gTmrPinNotUsedForOutComp_c;

    if (t == 1) {
        (void) Tmr_SetCallbackFunc(gTmr3_c, gTmrOverEvent_c, TimerCallBack1);
    } else {
        (void) Tmr_SetCallbackFunc(gTmr3_c, gTmrOverEvent_c, TimerCallBack2);
    }
    (void) Tmr_SetClkConfig(gTmr3_c, gTmrBusRateClk_c, gTmrClkDivBy128_c); //62500 cuentas por segundo
    (void) Tmr_SetChannelConfig(gTmr3_c, &timerConfig);
    (void) Tmr_Enable(gTmr3_c,gTmrClkDivBy128_c, timerCount);
}

void
uMac_Engine (void)
{   
    switch (uMac_Current_State) {
    case uMac_NoInit:
        if (uMac_On == TRUE) {
            uMac_On = FALSE;
            
            if (ui == 16) {
                ui = 0;
            }
            
            if (send_attempts > MAX_SEND_ATTEMPTS) {
                ui++;
                send_attempts = 0;
            }

            (void) MLMESetChannelRequest(Channels[ui]);
            
            if (MLMEEnergyDetect() > NOISE_ENERGY) {
                ui++;
                send_attempts = 0;
                uMac_TxPacket->Dest_Add = uMacbroad;
                uMac_TxPacket->Packet_Type = 0;
                uMac_TxPacket->Pan_ID = 10;
                uMac_TxPacket->Source_Add = MyID;
                (void) MCPSDataRequest(AppTxPacket);
                uMac_Current_State = uMac_Init;
            } else {
                send_attempts++;
            }
        }
        break;
    case uMac_Init:
        if (bTxDone == TRUE) {
            (void) MLMERXEnableRequest(AppRxPacket, 0);
            (void) SetTimer(10000);
            uMac_Current_State = uMac_WaitRx;
        }
        break;
    case uMac_WaitRx:
        if (bTimerFlag == TRUE) {
            bTimerFlag = FALSE;
            (void) MLMERXDisableRequest();
            uMac_Current_State = uMac_NoInit;
            uMac_On = TRUE;
        } else if (bRxDone == TRUE) {
            bRxDone = FALSE;
            if (AppRxPacket->rxStatus == rxSuccessStatus_c) {
                if (uMac_RxPacket->Pan_ID == 10) {
                    if (uMac_RxPacket->Dest_Add == MyID || uMac_RxPacket->Dest_Add == uMacbroad) {
                        switch (uMac_RxPacket->Source_Add) {
                        case 0:
                            Led_PrintValue(0x08);
                            break;
                        case 1:
                            Led_PrintValue(0x06);
                            break;
                        case 2:
                            Led_PrintValue(0x03);
                            break;
                        }
                        
                        if (uMac_RxPacket->Packet_Type != 0) {
                            // Llamar callback
                        }
                    }
                }
            }
            (void) MLMERXEnableRequest(AppRxPacket, 0);
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
                (void) SetTimer(MyID*62);
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
            (void) MLMERXEnableRequest(AppRxPacket, 0);
            uMac_Current_State = uMac_WaitRx;
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
TimerCallBack1 (void)
{
    bTimerFlag1 = TRUE;
    (void) Tmr_Disable(gTmr3_c); 
}

void
TimerCallBack2 (void)
{
    bTimerFlag2 = TRUE;
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