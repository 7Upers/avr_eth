/*********************************************
 * vim:sw=8:ts=8:si:et
 * To use the above modeline in vim you must have "set modeline" in your .vimrc
 * Author: Guido Socher
 * Copyright: GPL V2
 *
 * Ethernet remote device and sensor: test program 1
 *
 * Title: Microchip ENC28J60 Ethernet Interface Driver
 * Chip type           : ATMEGA88 with ENC28J60
 *********************************************/
#include <stdio.h>
#include <avr/io.h>
#include "ip_arp_udp.h"
#include "enc28j60.h"
#include "timeout.h"
#include "avr_compat.h"
#include "net.h"

// please modify the following two lines. mac and ip have to be unique
// in your local area network. You can not have the same numbers in
// two devices:
static uint8_t mymac[6] = {0x54,0x55,0x58,0x10,0x00,0x24}; 
static uint8_t myip[4] = {10,0,0,24};
// how did I get the mac addr? Translate the first 3 numbers into ascii is: TUX

#define BUFFER_SIZE 250
static uint8_t buf[BUFFER_SIZE+1];

#define BAUD 19200
#define MYUBRR F_CPU/16/BAUD-1

void uart_init(unsigned int ubrr)
{
	//set speed
	UBRR0H = (unsigned char)(ubrr>>8);
	UBRR0L = (unsigned char)(ubrr);
	/* Enable receiver and transmitter */
	UCSR0B = (1<<RXEN0)|(1<<TXEN0);
	/* Set frame format: 8data, 1stop bit - default*/
}

char uart_getchar(void)
{
	/* Wait for data to be received */
	while (!(UCSR0A & (1<<RXC0)));
	return UDR0;
}

void uart_putchar(char c)
{
	while (!(UCSR0A & (1<<UDRE0))); /* Wait until transmission ready. */
	UDR0 = c;
}


FILE uart_io = FDEV_SETUP_STREAM(uart_putchar, uart_getchar, _FDEV_SETUP_RW);


int main(void)
{
	uart_init(MYUBRR);
	stdin = &uart_io;
	stdout = &uart_io;

	uint16_t plen;

	// set the clock speed to 8MHz
	// set the clock prescaler. First write CLKPCE to enable setting of clock the
	// next four instructions.
	CLKPR=(1<<CLKPCE);
	CLKPR=(1<<CLKPS0); // 16/2=8 MHZ

        /* enable PB0, reset as output */
        DDRB|= (1<<PB0);
        /* set output to gnd, reset the ethernet chip */
        PORTB &= ~(1<<PB0);
        delay_ms(20);
        /* set output to Vcc, reset inactive */
        PORTB|= (1<<PB0);
        delay_ms(100);

	//initialize enc28j60
	enc28j60Init(mymac);
	printf("ethernet inited\r\n");
/*
	printf("start test enc28j60 communication\r\n");

	//MAADR0
	uint8_t address = 0x01|0x60|0x80;

	enc28j60SetBank(address);

	//CSACTIVE;
	PORTB &= ~(1<<PB2);
        // issue read command
        SPDR = 0x00 | (address & 0x1F);
        printf("sent address 0x%x\r\n",address);
	//waitspi();
	while(!(SPSR&(1<<SPIF)));
        // read data
        SPDR = 0x00;
        printf("sent cmd read data\r\n");
	while(!(SPSR&(1<<SPIF)));
        // do dummy read if needed (for mac and mii, see datasheet page 29)
        if(address & 0x80)
        {
                SPDR = 0x00;
		while(!(SPSR&(1<<SPIF)));
        }
        // release CS
	//CSPASSIVE;
	PORTB |= (1<<PB2);
	printf("enc28j60 deactivated\r\n");
	printf ("enc28j60 return 0x%02x\r\n",SPDR);
*/
	/* Magjack leds configuration, see enc28j60 datasheet, page 11 */
	// LEDA=greed LEDB=yellow
	//
	// 0x880 is PHLCON LEDB=on, LEDA=on
	// enc28j60PhyWrite(PHLCON,0b0000 1000 1000 00 00);
	enc28j60PhyWrite(PHLCON,0x880);
	printf("leds on\r\n");
	delay_ms(500);
	//
	// 0x990 is PHLCON LEDB=off, LEDA=off
	// enc28j60PhyWrite(PHLCON,0b0000 1001 1001 00 00);
	enc28j60PhyWrite(PHLCON,0x990);
	printf("leds off\r\n");
	delay_ms(500);
	//
	// 0x880 is PHLCON LEDB=on, LEDA=on
	// enc28j60PhyWrite(PHLCON,0b0000 1000 1000 00 00);
	enc28j60PhyWrite(PHLCON,0x880);
	printf("leds on\r\n");
	delay_ms(500);
	//
	// 0x990 is PHLCON LEDB=off, LEDA=off
	// enc28j60PhyWrite(PHLCON,0b0000 1001 1001 00 00);
	enc28j60PhyWrite(PHLCON,0x990);
	printf("leds off\r\n");
	delay_ms(500);
	//
        // 0x476 is PHLCON LEDA=links status, LEDB=receive/transmit
        // enc28j60PhyWrite(PHLCON,0b0000 0100 0111 01 10);
	enc28j60PhyWrite(PHLCON,0x476);
	printf("leds functions\r\n");
	delay_ms(100);

	//init the ethernet/ip layer:
	init_ip_arp_udp(mymac,myip);
	printf("arp ip udp inited\r\n");

        while(1){
                // get the next new packet:
                plen = enc28j60PacketReceive(BUFFER_SIZE, buf);

                /*plen will ne unequal to zero if there is a valid 
                 * packet (without crc error) */
                if(plen==0){
                        continue;
                }
                // arp is broadcast if unknown but a host may also
                // verify the mac address by sending it to 
                // a unicast address.
                if(eth_type_is_arp_and_my_ip(buf,plen)){
                        make_arp_answer_from_request(buf,plen);
                        continue;
                }
                // check if ip packets (icmp or udp) are for us:
                if(eth_type_is_ip_and_my_ip(buf,plen)==0){
                        continue;
                }
                
                if(buf[IP_PROTO_P]==IP_PROTO_ICMP_V && buf[ICMP_TYPE_P]==ICMP_TYPE_ECHOREQUEST_V){
                        // a ping packet, let's send pong
                        make_echo_reply_from_request(buf,plen);
                        continue;
                }
        }
        return (0);
}
