/*
IBEX UK LTD http://www.ibexuk.com
Electronic Product Design Specialists
RELEASED SOFTWARE

The MIT License (MIT)

Copyright (c) 2013, IBEX UK Ltd, http://ibexuk.com

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
//Project Name:		PIC32 Half duplex UART Driver




//ALL COMMS ARE DONE BY LOADING A BUFFER AND JUST LETTING THESE FUNCTIONS GET ON WITH IT

#include "main.h"					//Global data type definitions (see https://github.com/ibexuk/C_Generic_Header_File )
#define AP_COMMS
#include "ap-comms.h"



//***********************************
//***********************************
//********** PROCESS COMMS **********
//***********************************
//***********************************
void comms_process (void)
{
	BYTE data;
	WORD command;
	WORD data_length;
	BYTE tx_response = 0;
	BYTE *p_buffer;
	WORD w_temp;


	if (!comms_rx_process_packet)
		return;


	data_length = (WORD)comms_tx_rx_buffer[0] << 8;
	data_length |= (WORD)comms_tx_rx_buffer[1];
	
	command = (WORD)comms_tx_rx_buffer[2] << 8;
	command |= (WORD)comms_tx_rx_buffer[3];

	switch (command)
	{

	case CMD_GET_STATUS_REQUEST:
		//----------------------
		//----------------------
		//----- GET STATUS -----
		//----------------------
		//----------------------

		//----- SEND REPONSE -----
		p_buffer = &comms_tx_rx_buffer[2];
		*p_buffer++ = CMD_GET_STATUS_RESPONSE >> 8;				//Command
		*p_buffer++ = CMD_GET_STATUS_RESPONSE & 0x00ff;

		//Our firmware version
		//*p_buffer++ = OUR_FIRMWARE_REVISION >> 8;				//Data
		//*p_buffer++ = OUR_FIRMWARE_REVISION & 0x00ff;
		
		//Pad remaining bytes
		while (p_buffer < (&comms_tx_rx_buffer[4] + 64))
			*p_buffer++ = 0x00;

		tx_response = 1;
		break;
		
		


		
	//<<<< ADD MORE COMMANDS HERE
		
		
	} //switch (command)

	comms_rx_process_packet = 0;
	if (tx_response)
	{
		//-----------------------------
		//----- TRANSMIT RESPONSE -----
		//-----------------------------
		w_temp = p_buffer - &comms_tx_rx_buffer[0];
		comms_tx_rx_buffer[0] = w_temp >> 8;
		comms_tx_rx_buffer[1] = w_temp & 0x00ff;
		
		
		//ENABLE TX IRQ
		comms_tx_no_of_bytes_to_tx = w_temp;
		comms_tx_byte = 0;

		INTClearFlag(INT_SOURCE_UART_TX(COMMS_UART_NAME));
		UARTSendDataByte(COMMS_UART_NAME, comms_tx_buffer[comms_tx_byte++]);		//Manually trigger the first tx
		INTEnable(INT_SOURCE_UART_TX(COMMS_UART_NAME), INT_ENABLED);
	}
	else
	{
		//-----------------------
		//----- NO RESPONSE -----
		//-----------------------
	
		//----- SET BACK TO RECEIVE -----
		//ENABLE RX IRQ
		comms_rx_byte = 0;
	  	UARTSetLineControl(COMMS_UART_NAME, COMMS_UART_LINE_CONTROL);		//Reset rx
	   	
		while (UARTReceivedDataIsAvailable(COMMS_UART_NAME))				//Clear RX buffer of any data
			data = UARTGetDataByte(COMMS_UART_NAME);
	
		INTClearFlag(INT_SOURCE_UART_RX(COMMS_UART_NAME));
		INTEnable(INT_SOURCE_UART_RX(COMMS_UART_NAME), INT_ENABLED);
		INTClearFlag(INT_SOURCE_UART_ERROR(COMMS_UART_NAME));
		INTEnable(INT_SOURCE_UART_ERROR(COMMS_UART_NAME), INT_ENABLED);
	}

}






//********************************
//********************************
//********** INTERRUPTS **********
//********************************
//********************************
void __ISR(_UART_1_VECTOR, ipl5) Uart1InterruptHandler (void) 		//(ipl# must match the priority level assigned to the irq where its enabled)
{
	static BYTE data;
	static BYTE status;
	static WORD_VAL tx_checksum;
	static WORD_VAL rx_checksum;
	static WORD_VAL rx_checksum_received;

	Nop();
	
	if (
	(INTGetEnable(INT_SOURCE_UART_TX(COMMS_UART_NAME))) &&
	(INTGetFlag(INT_SOURCE_UART_TX(COMMS_UART_NAME)))
	)
	{
		//----------------------------------
		//----------------------------------
		//---------- TX INTERRUPT ----------
		//----------------------------------
		//----------------------------------

		while (UARTTransmitterIsReady(COMMS_UART_NAME))
		{

			//----- SEND NEXT BYTE -----
			if (comms_tx_byte == 0)
			{
				//--------------------
				//----- 1ST BYTE -----
				//--------------------
				tx_checksum.Val = comms_tx_rx_buffer[comms_tx_byte];
				UARTSendDataByte(COMMS_UART_NAME, comms_tx_rx_buffer[comms_tx_byte]);
				comms_tx_byte++;
			}
			else if (comms_tx_byte < comms_tx_no_of_bytes_to_tx)
			{
				//---------------------
				//----- NEXT BYTE -----
				//---------------------
				tx_checksum.Val += comms_tx_rx_buffer[comms_tx_byte];
				UARTSendDataByte(COMMS_UART_NAME, comms_tx_rx_buffer[comms_tx_byte]);
				comms_tx_byte++;
			}
			else if (comms_tx_byte == comms_tx_no_of_bytes_to_tx)
			{
				//----------------------
				//----- CHECKSUM H -----
				//----------------------
				UARTSendDataByte(COMMS_UART_NAME, tx_checksum.v[1]);
				comms_tx_byte++;
			}
			else if (comms_tx_byte > comms_tx_no_of_bytes_to_tx)
			{
				//----------------------
				//----- CHECKSUM L -----
				//----------------------
				UARTSendDataByte(COMMS_UART_NAME, tx_checksum.v[0]);
				comms_tx_byte++;
	
				//----- ALL BYTES SENT -----
				comms_tx_byte = 0;						//Reset tx byte counter
				comms_rx_byte = 0;
				comms_tx_no_of_bytes_to_tx = 0;
	
				//DISABLE TX IRQ
				INTEnable(INT_SOURCE_UART_TX(COMMS_UART_NAME), INT_DISABLED);
	
				//ENABLE RX IRQ
				comms_rx_byte = 0;
	    		UARTSetLineControl(COMMS_UART_NAME, COMMS_UART_LINE_CONTROL);		//Reset rx
		    	
				while (UARTReceivedDataIsAvailable(COMMS_UART_NAME))				//Clear RX buffer of any data
					data = UARTGetDataByte(COMMS_UART_NAME);
			
				INTClearFlag(INT_SOURCE_UART_RX(COMMS_UART_NAME));
				INTEnable(INT_SOURCE_UART_RX(COMMS_UART_NAME), INT_ENABLED);
				INTClearFlag(INT_SOURCE_UART_ERROR(COMMS_UART_NAME));
				INTEnable(INT_SOURCE_UART_ERROR(COMMS_UART_NAME), INT_ENABLED);
				
				break;
			}
			
		} //while (UARTTransmitterIsReady(COMMS_UART_NAME))
		INTClearFlag(INT_SOURCE_UART_TX(COMMS_UART_NAME));		//Do after sending bytes
	}



	if (
	(INTGetEnable(INT_SOURCE_UART_ERROR(COMMS_UART_NAME))) &&
	(INTGetFlag(INT_SOURCE_UART_ERROR(COMMS_UART_NAME)))
	)
	{
		//----------------------------------------
		//----------------------------------------
		//---------- RX ERROR INTERRUPT ----------
		//----------------------------------------
		//----------------------------------------

		status = UARTGetLineStatus(COMMS_UART_NAME);
		if (status & UART_PARITY_ERROR)
		{
			Nop();
		}
		if (status & UART_FRAMING_ERROR)
		{
			Nop();
		}
		if (status & UART_OVERRUN_ERROR)			//OERR must be cleared, clearing will reset RX FIFO
		{
			COMMS_UART_STATUS = (COMMS_UART_STATUS & ~UART_OVERRUN_ERROR);		//Clear OERR bit
		}
    	
    	//Clear RX buffer of bad data
		while (UARTReceivedDataIsAvailable(COMMS_UART_NAME))
			data = UARTGetDataByte(COMMS_UART_NAME);
		INTClearFlag(INT_SOURCE_UART_RX(COMMS_UART_NAME));
		
		comms_rx_byte = 0xffff;
	}



	if (
	(INTGetEnable(INT_SOURCE_UART_RX(COMMS_UART_NAME))) &&
	(INTGetFlag(INT_SOURCE_UART_RX(COMMS_UART_NAME)))
	)
	{
		//----------------------------------
		//----------------------------------
		//---------- RX INTERRUPT ----------
		//----------------------------------
		//----------------------------------

		//WORD size (excluding 2 byte checksum)
		//WORD command
		//BYTE[] data
		//WORD checksum

		if (comms_rx_reset_1ms_timer == 0)
			comms_rx_byte = 0;
		
		comms_rx_reset_1ms_timer = 20;			//<<<< Gap required between packets to mark start of packet

		while (UARTReceivedDataIsAvailable(COMMS_UART_NAME))
		{
			//----- GET DATA BYTE -----
			data = UARTGetDataByte(COMMS_UART_NAME);

			if (comms_rx_byte == 0)
			{
				//--------------------
				//----- LENGTH H -----
				//--------------------
				comms_rx_no_of_bytes_to_rx = (WORD)data << 8;
				comms_tx_rx_buffer[comms_rx_byte++] = data;
				
				rx_checksum.Val = (WORD)data;
			}
			else if (comms_rx_byte == 1)
			{
				//--------------------
				//----- LENGTH L -----
				//--------------------
				comms_rx_no_of_bytes_to_rx |= (WORD)data;
				comms_tx_rx_buffer[comms_rx_byte++] = data;
				
				rx_checksum.Val += (WORD)data;
				
				if (comms_rx_no_of_bytes_to_rx >= LENGTH_OF_USART_BUFFER)
					comms_rx_byte = 0xffff;
			}
			else if (comms_rx_byte < comms_rx_no_of_bytes_to_rx)
			{
				//--------------------------
				//----- NEXT DATA BYTE -----
				//--------------------------
				comms_tx_rx_buffer[comms_rx_byte++] = data;
				rx_checksum.Val += (WORD)data;
			}
			else if (comms_rx_byte == comms_rx_no_of_bytes_to_rx)
			{
				//----------------------
				//----- CHECKSUM H -----
				//----------------------
				rx_checksum_received.v[1] = data;
				comms_rx_byte++;
			}
			else if (comms_rx_byte == (comms_rx_no_of_bytes_to_rx + 1))
			{
				//----------------------
				//----- CHECKSUM H -----
				//----------------------
				rx_checksum_received.v[0] = data;
				comms_rx_byte++;
				
				if (rx_checksum.Val == rx_checksum_received.Val)
				{
					//---------------------------------
					//----- VALID PACKET RECEIVED -----
					//---------------------------------
					comms_rx_process_packet = 1;					//Flag for main function to process rx

					//Cleasr RX IRQ
					INTEnable(INT_SOURCE_UART_RX(COMMS_UART_NAME), INT_DISABLED);
					INTEnable(INT_SOURCE_UART_ERROR(COMMS_UART_NAME), INT_DISABLED);
					break;
				}
			}

		}
		INTClearFlag(INT_SOURCE_UART_RX(COMMS_UART_NAME));				//Do after reading data as any data in RX FIFO will stop irq bit being cleared
	}
}



