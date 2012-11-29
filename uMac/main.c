/*****************************************************************************
* Generic App demo main file.
* 
* Copyright (c) 2010, Freescale, Inc. All rights reserved.
*
* 
* No part of this document must be reproduced in any form - including copied,
* transcribed, printed or by any electronic means - without specific written
* permission from Freescale Semiconductor.
*
*  The Generic App demo may be used as template to develop proprietary solutions.
*  This applications shows all the basic initialization required to start the 
*  Radio, run SMAC, and the commonly used peripherals such as UART, LEDs, and 
*  KBIs.
*
*****************************************************************************/


#include <hidef.h>                  /*EnableInterrupts macro*/
#include "McuInit.h"                /*CPU and System Clock related functions*/
#include "derivative.h"             /*System Clock related declarations*/
#include "EmbeddedTypes.h"          /*Include special data types*/             
#include "Utilities_Interface.h"    /*Include Blocking Delays and data conversion functions*/
#include "PLM_config.h"
#include "SMAC_Interface.h"         /*Include all OTA functionality*/
#include "Radio_Interface.h"        /*Include all Radio functionality*/
#include "app_config.h"
#include "OTAP_Interface.h"
#include "uMAc_Interface.h"


/************************************************************************************
*************************************************************************************
* Private prototypes
*************************************************************************************
************************************************************************************/
void InitProject(void);
//void InitSmac(void);

/* Place it in NON_BANKED memory */
#ifdef MEMORY_MODEL_BANKED
#pragma CODE_SEG __NEAR_SEG NON_BANKED
#else
#pragma CODE_SEG DEFAULT
#endif /* MEMORY_MODEL_BANKED */


void MLMEScanComfirm(channels_t ClearestChann);
void MLMEResetIndication(void);
void MLMEWakeComfirm(void);
void UartRxCallback(uint8_t u8UartFlags);
void UartTxCallback(void);
#if (TRUE == gKeyboardSupported_d) || (TRUE == gTouchpadSupported_d) || (TRUE == gKbiSupported_d)
  void KbiCallback(kbiPressed_t PressedKey);
#endif
#if (gTargetBoard_c == gMc1323xRcm_c) || (gTargetBoard_c == gMc1323xRem_c)
  void KeyboardCallback (keyboardButton_t keyPressed);
#endif
#if gTargetBoard_c == gMc1323xRcm_c
  void TouchpadCallback(touchpadEvent_t * event);
#endif
void LCDCallback(lcdErrors_t lcdError);

void MCPSDataComfirm(txStatus_t TransmissionResult);
void MCPSDataIndication(rxPacket_t *gsRxPacket);


/* Place your callbacks prototypes declarations here */

#pragma CODE_SEG DEFAULT



/************************************************************************************
*************************************************************************************
* Public memory declarations
*************************************************************************************
************************************************************************************/

bool_t  KeyPressed = FALSE;

bool_t bUartDataInFlag;
bool_t bUartTxDone;  
uint8_t UartData;
uartPortNumber_t portNumber;  
uartConfigSet_t uartSettings;

kbiConfig_t gKbiConfiguration;

uMac_nodeType type;
uint8_t dest;


/************************************************************************************
*************************************************************************************
* Main application functions
*************************************************************************************
************************************************************************************/

void main(void) 
{

  MCUInit();
  EnableInterrupts; /* Enable interrupts */  
  
  InitProject();
  //InitSmac();
  (void) Init_uMac();
  //(void) uMac_Txf();
  (void)Uart_BlockingStringTx("\f\r\n\r\n\t Generic Demonstration Application", gDefaultUartPort_c);
 
  for(;;) 
  {
    Otap_OpcMain();
     
    /* Put your own code here */
    //if (KeyPressed == TRUE) {
    	
    	(void) uMac_Engine();
    //}
    __RESET_WATCHDOG();
    
  } 
 
}


/************************************************************************************
*
* InitProject
*
************************************************************************************/
void InitProject(void)
{
  /* GPIO Initialization */ 
  Gpio_Init();
    
  /* UART Initialization */
#if TRUE == gUartSupported_d   
  portNumber = gDefaultUartPort_c; 
  uartSettings.baudRate = gUartDefaultBaud_c;
  uartSettings.dataBits = g8DataBits_c;
  uartSettings.parity = gUartParityNone_c;
  (void)Uart_Init();
  (void)Uart_Configure(portNumber, &uartSettings);
  (void)Uart_RegisterRxCallBack(UartRxCallback, gUartRxCbCodeNewByte_c, portNumber);
  (void)Uart_RegisterTxCallBack(UartTxCallback, portNumber);
  
  
#endif

  
 /* Timer Initialization */    
  (void)Tmr_Init(); 
  
 /* KBI Initialization */
  #if gTargetBoard_c == gMc1323xRcm_c || gTargetBoard_c == gMc1323xRem_c  
  #if (gKeyboardSupported_d || gTouchpadSupported_d)    
    gKbiConfiguration.Control.bit.TriggerByLevel = 0;
    gKbiConfiguration.Control.bit.Interrupt = 1;
    gKbiConfiguration.Control.bit.Acknowledge = 1;
    gKbiConfiguration.InterruptPin.Port = gSwitchColmnMask_c|gTouchpadAttnPinMask_c;
    gKbiConfiguration.EdgeSelect.Port = gSwitchColmnMask_c;
    (void)Kbi_Init((kbiCallback_t)KbiCallback, &gKbiConfiguration, gSwitchKbiModule_c);
    (void)Keyboard_InitKeyboard(KeyboardCallback);
  #endif
  #else
    gKbiConfiguration.Control.bit.TriggerByLevel = 0;
    gKbiConfiguration.Control.bit.Interrupt = 1;
    gKbiConfiguration.Control.bit.Acknowledge = 1;
    gKbiConfiguration.InterruptPin.Port = gSwitchMask;
    gKbiConfiguration.EdgeSelect.Port = 0;
    (void)Kbi_Init((kbiCallback_t)KbiCallback, &gKbiConfiguration, gSwitchKbiModule_c);
  #endif 

  /* SPI Initialization */ 
  SPI1_Init(gSpiBaudDivisor_2_c); 

  #if gTargetBoard_c == gMc1321xSrb_c || gTargetBoard_c == gMc1321xNcb_c || gTargetBoard_c == gMc1320xS08qe128Evb_c 
    IRQ_Init(NULL);
  #endif
  
  /* Touchpad Initialization */ 
  #if gTargetBoard_c == gMc1323xRcm_c
  (void)IIC_Init(mIic100KhzBaudInitParameters_c);
  (void)Touchpad_DriverInit(TouchpadCallback, gGpioPortB_c, gGpioPin6Mask_c);
  #endif
  
  /*LCD configuration*/
  (void)Lcd_Init(LCDCallback);
  (void)Lcd_Config(TRUE,TRUE,FALSE);


   /* OTAP Initialization */
  Otap_OpcInit();
    
  /* Place your app initialization here */


}

/************************************************************************************
*
* InitSmac
*
************************************************************************************/

/* Place it in NON_BANKED memory */
#ifdef MEMORY_MODEL_BANKED
#pragma CODE_SEG __NEAR_SEG NON_BANKED
#else
#pragma CODE_SEG DEFAULT
#endif /* MEMORY_MODEL_BANKED */

/************************************************************************************
* User's Callbacks
************************************************************************************/

/* Place your callbacks here */







/************************************************************************************
*
* TouchpadCallback
*
************************************************************************************/
#if gTargetBoard_c == gMc1323xRcm_c
void TouchpadCallback(touchpadEvent_t * event)
{
  switch(event->EventType)
  {
    case gTouchpadBusError_c:
         /* Place your implementation here */ 
    break;
    case gTouchpadGpioEvent_c:
         /* Place your implementation here */
    break;
    case gTouchpadFingerPositionEvent_c:
         /* Place your implementation here */
    break;
    case gTouchpadPinchGestureEvent_c:
         /* Place your implementation here */ 
    break;
    case gTouchpadFlickGestureEvent_c:  
         /* Place your implementation here */
    break;
    case gTouchpadEarlyTapGestureEvent_c:
         /* Place your implementation here */
    break;
    case gTouchpadDoubleTapGestureEvent_c:
         /* Place your implementation here */
    break;
    case gTouchpadTapAndHoldGestureEvent_c:
         /* Place your implementation here */
    break;
    case gTouchpadSingleTapGestureEvent_c:
         /* Place your implementation here */
    break;
    case gTouchpadDevStatusEvent_c:
         /* Place your implementation here */
    break;
    case gTouchpadFlashEvent_c:
         /* Place your implementation here */
    break;
    default:
         /* Place your implementation here */ 
    break;
  }
}
#endif


/************************************************************************************
* UartTxCallback
* 
*
*
************************************************************************************/
void UartTxCallback(void)
{
    bUartTxDone = TRUE;  
}

/************************************************************************************
* UartRxCallback
* 
*
*
************************************************************************************/
void UartRxCallback(uint8_t u8UartFlags)
{
  uint8_t iByteNumber;
  (void)u8UartFlags;
  
  iByteNumber = 1;
  
  (void)Uart_GetBytesFromRxBuffer(&UartData, &iByteNumber, portNumber);
  bUartDataInFlag = TRUE;
 
}

#if (TRUE == gKeyboardSupported_d) || (TRUE == gTouchpadSupported_d) || (TRUE == gKbiSupported_d)
/************************************************************************************
* KbiCallback
* 
*  This function should be set as the Kbi callback function in Kbi_Init
*
************************************************************************************/
#if gTargetBoard_c == gMc1323xRcm_c || gTargetBoard_c == gMc1323xRem_c
void KbiCallback(kbiPressed_t PressedKey)
{  
  (void)PressedKey;
  if(gKbiPressedKey0_c == PressedKey || gKbiPressedKey1_c == PressedKey || gKbiPressedKey2_c == PressedKey || gKbiPressedKey3_c == PressedKey \
     || gKbiPressedKey4_c == PressedKey || gKbiPressedKey5_c == PressedKey)
  {
     Keyboard_KbiEvent(PressedKey);
  }
  else if (gKbiPressedKey6_c == PressedKey)
  {
     #if gTargetBoard_c == gMc1323xRcm_c
     Touchpad_EventHandler();
     #endif
  }
   
} 
#else
void KbiCallback(kbiPressed_t PressedKey)
{  
  KeyPressed = TRUE;
  switch(PressedKey)
  {
    case gKbiPressedKey0_c:
      /* Place your implementation here */
    	type = uMac_Router;
    break;
    case gKbiPressedKey1_c:
      /* Place your implementation here */
    	type = uMac_Client;
    break;
    case gKbiPressedKey2_c:
      /* Place your implementation here */
    	dest = 1;
    break;
    case gKbiPressedKey3_c:
      /* Place your implementation here */
    	dest = 2;
    break;
    default:
    break;
  }
   
}


#endif 
      
#endif

/************************************************************************************
* KeyboardCallback
* 
*
*
************************************************************************************/
#if (gTargetBoard_c == gMc1323xRcm_c) || (gTargetBoard_c == gMc1323xRem_c)

void KeyboardCallback(keyboardButton_t keyPressed)
{
   KeyPressed = TRUE;
   switch(keyPressed)
   {
    case gSw1_c:
      /* Place your implementation here */ 
    break;
    
    case gSw2_c:
      /* Place your implementation here */
    break;
    
    case gSw3_c:
      /* Place your implementation here */ 
    break;
    
    case gSw4_c:
      /* Place your implementation here */ 
    break;
    
    case gSw5_c:
      /* Place your implementation here */ 
    break;
    
    case gSw6_c:
      /* Place your implementation here */ 
    break;
    
    case gSw7_c:
      /* Place your implementation here */ 
    break;
    
    case gSw8_c:
      /* Place your implementation here */ 
    break;

#if gTargetBoard_c == gMc1323xRcm_c     
    case gSw9_c:
      /* Place your implementation here */ 
    break;
    
    case gSw10_c:
      /* Place your implementation here */ 
    break;
    
    case gSw11_c:
      /* Place your implementation here */
    break;
    
    case gSw12_c:
      /* Place your implementation here */ 
    break;
    
    case gSw13_c:
      /* Place your implementation here */ 
    break;
    
    case gSw14_c:
      /* Place your implementation here */
    break;
    
    case gSw15_c:
      /* Place your implementation here */
    break;
    
    case gSw16_c:
      /* Place your implementation here */ 
    break;
    
    case gSw17_c:
      /* Place your implementation here */ 
    break;
    
    case gSw18_c:
      /* Place your implementation here */
    break;
    
    case gSw19_c:
      /* Place your implementation here */
    break;
    
    case gSw20_c:
      /* Place your implementation here */ 
    break;
    
    case gSw21_c:
      /* Place your implementation here */ 
    break;
    
    case gSw22_c:
      /* Place your implementation here */ 
    break;
    
    case gSw23_c:
      /* Place your implementation here */
    break;
    
    case gSw24_c:
      /* Place your implementation here */ 
    break;
    
    case gSw25_c:
      /* Place your implementation here */ 
    break;
    
    case gSw26_c:
      /* Place your implementation here */ 
    break;
    
    case gSw27_c:
      /* Place your implementation here */ 
    break;
    
    case gSw28_c:
      /* Place your implementation here */ 
    break;
    
    case gSw29_c:
      /* Place your implementation here */ 
    break;
    
    case gSk1_c:
      /* Place your implementation here */ 
    break;
    
    case gSk2_c:
      /* Place your implementation here */ 
    break;
    
    case gSk3_c:
      /* Place your implementation here */ 
    break;
    
    case gSk4_c:
      /* Place your implementation here */ 
    break;
    
#endif    
    
    default:
      /* Place your implementation here */ 
    break;
   }
       
}
#endif

/************************************************************************************
* LCDCallback
* 
*
*
************************************************************************************/
void LCDCallback(lcdErrors_t lcdError)
{
  (void)lcdError;
}


/************************************************************************************
* SMAC Callbacks
************************************************************************************/

/************************************************************************************
* MCPSDataComfirm
* 
*
*
************************************************************************************/
 

/************************************************************************************
* MCPSDataIndication
* 
*
*
************************************************************************************/



/************************************************************************************
* MLMEScanComfirm
* 
*
*
************************************************************************************/


/************************************************************************************
* MLMEResetIndication
* 
*
*
************************************************************************************/


/************************************************************************************
* MLMEWakeComfirm
* 
*
*
************************************************************************************/




#pragma CODE_SEG DEFAULT




