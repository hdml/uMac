/*
 * uMac_Interface.h
 *
 *  Created on: Nov 1, 2012
 */

#ifndef UMAC_INTERFACE_H_
#define UMAC_INTERFACE_H_

#endif /* UMAC_INTERFACE_H_ */



//Tipos Publicos
typedef enum{
  uMac_Router,
  uMac_Client,
  uMac_Other
} uMac_nodeType;

/*typedef enum{
	uMac_Hello,
	uMac_Data,
	uMac_Other1
}uMac_PacketType;*/

typedef struct{
	uint8_t Pan_ID;
	uint8_t Packet_Type;
	uint8_t Source_Add;
	uint8_t Dest_Add;
	uint8_t uMac_PDU[1];
} uMac_Packet;

void NetworkTxCallback(void);
void NetworkRxCallback(void);

//Callbacks Publicos

#ifdef MEMORY_MODEL_BANKED
  typedef void __near(* __near uMac_txCallBack)(void);
  typedef void __near(* __near uMac_rxCallBack)(void);
#else
  typedef void(* __near uMac_txCallBack)(void);
  typedef void(* __near uMac_rxCallBack)(void);
#endif
  


//Prototipos Publicos
void Init_uMac(uMac_nodeType/*, uint8_tuMac_txCallBack,uMac_rxCallBack*/);
void uMac_Engine(void);

