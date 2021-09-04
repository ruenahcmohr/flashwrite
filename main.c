/*

1M chip is 1048576 bits ( 131072 bytes ) (last address 1FFFF)
2M chip is 2097152 bits ( 262144 bytes ) (last address 3FFFF)
4M chip is 4194304 bits ( 524288 bytes ) (last address 7FFFF)


todo:
  Make read accept start and end address
  Fix wait to check toggle lock and operation failure
   
  
*/

#include <avr/io.h>
#include "avrcommon.h"
#include "usart.h"

//port definitions
#define DATAPORT PORTA
#define DATAPIN  PINA
#define DATADDR  DDRA

#define CTRLPORT PORTD
#define CTRLPIN  PIND
#define CTRLDDR  DDRD

#define CS       5
#define WR       7
#define RD       6


#define OELOW()    ClearBit(RD, CTRLPORT)
#define OEHIGH()   SetBit(RD, CTRLPORT)

#define WRLOW()    ClearBit(WR, CTRLPORT)
#define WRHIGH()   SetBit(WR, CTRLPORT)

#define CSLOW()    ClearBit(CS, CTRLPORT)
#define CSHIGH()   SetBit(CS, CTRLPORT)

#define IDLE()     CSHIGH(); WRHIGH(); RDHIGH();

#define WRPULSE()  CSLOW(); WRLOW(); NOP(); WRHIGH(); CSHIGH();
#define RDSTART()  CSLOW(); OELOW(); NOP();
#define RDFINISH() OEHIGH(); CSHIGH();

#define NibToBin(A) (((A)>'9')?((A)-'7'):((A)-'0'))

void Delay(unsigned int delay) {
  unsigned int x;
  for (x = delay; x != 0; x--) {
    asm volatile ("nop"::); 
  }
}

void Delay2(unsigned int delay) {
  int x;
  for (x = delay; x != 0; x--) {
    Delay(65535); 
  }
}

void setAddress(unsigned long address) {
  PORTB =  address & 0xFF;                  // A0-7
  PORTC =  (address >> 8) & 0xFF;           // A8-15
  if (address & ((unsigned long)1<<16)) SetBit(2, PORTD);  // A16
  else ClearBit(2, PORTD);
  if (address & ((unsigned long)1<<17)) SetBit(3, PORTD);  // A17
  else ClearBit(3, PORTD);
}

void write(unsigned long address, char data) {
  DATADDR = 0xFF;
  DATAPORT =  data;                            // data bits
  setAddress(address);
  WRPULSE();                                // do!
  DATAPORT = 0x00;   // no residual on the bus
  DATADDR = 0x00;
}

unsigned char read(unsigned long address) {
  unsigned char temp;

  setAddress(address);
  RDSTART();
  temp = PINA;
  RDFINISH();
  return temp;
  
}

void unlock() {
  write(0x555, 0xAA);
  write(0x2AA, 0x55);
}

void chipErase() {
  unlock();
  write(0x555, 0x80);
  unlock();
  write(0x555, 0x10);
}

unsigned char getID() {
  unlock();
  write(0x555, 0x90);
  return read(0x001);
}

void program(unsigned long address, char data) {
  unlock();
  write(0x555, 0xA0);
  write(address, data);    
}

void DoRead() {
  unsigned char checksum;
  unsigned long address;
  unsigned int i;
  unsigned char temp;
  
  address = 0;
  
  write(0x00, 0xF0); // reset
  while(address < 0x3FFFF) {
  
    if ((address & 0xFFFF) == 0) {  // page record
      USART_printstring(":02000002");          checksum = 4; 
      USART_printhex((address>>12) & 0xFF);    checksum += ((address>>12) & 0xFF);
      USART_printhex((address>>4) & 0xFF);     checksum += ((address>>4) & 0xFF);
      USART_printhex(0-checksum);
      USART_printstring("\r\n"); 
    }
  
    USART_printstring(":10");             checksum = 0x10;
    USART_printhex((address>>8) & 0xFF);  checksum += ((address>>8) & 0xFF);
    USART_printhex(address & 0xFF);       checksum += (address & 0xFF);
    USART_printstring("00");              
    for( i = 0; i < 0x10; i++) {
     temp = read(address+i);            
     USART_printhex(temp);                checksum += temp;
    }
    address += 0x10;
    
    USART_printhex(0-checksum);
    USART_printstring("\r\n");   
    
  }
  
  USART_printstring(":00000001FF\r\n"); // EOF

}

void printAddress( unsigned long address) {
  USART_printstring("0x");
  USART_printhex((address>>24) & 0xFF);
  USART_printhex((address>>16) & 0xFF);
  USART_printhex((address>>8) & 0xFF);
  USART_printhex(address & 0xFF);
}

void BlankCheck( void ) {

  unsigned long address;
  unsigned char temp;
  
  address = 0;
              
  for( address = 0; address < 0x40000; address++) {
    temp = read(address);                       
    if (temp != 0xFF) {
       USART_printstring("FAIL @ "); 
       printAddress( address);
       /* 
       USART_printhex((address>>24) & 0xFF);
       USART_printhex((address>>16) & 0xFF);
       USART_printhex((address>>8) & 0xFF);
       USART_printhex(address & 0xFF);
       */
       USART_printstring("\r\n");  
       return;
    }
  }
  USART_printstring("BLANK.\r\n");     
  
}

unsigned char get8(void) {
  unsigned char v;
  unsigned char t;
  t = USART_Receive(); v  = NibToBin(t) << 4;
  t = USART_Receive(); v |= NibToBin(t);
  return v;  
}

unsigned int get16(void) {
 unsigned int v;  
  v =  get8() << 8;
  v |= get8();  
  return v;
}

void waitReady(void) {  // this is a hack, 
  unsigned char p, s;
  
  p = read(0x00);
  s = ~p;
  while(s != p) {
    p = s;
    s = read(0x00);        
  }
  

}

void HexWrite(void) {
  unsigned char buffer[256];
  unsigned char checksum;
  unsigned char bytes, i;
  unsigned int  address;
  unsigned int  segment;
  unsigned char type;
  
  segment = 0;
  USART_printstring("READY FOR DATA:");
  while(1) {
    // Get the record.
    while( USART_Receive() != ':'); // wait for start
    bytes   = get8();            checksum =  bytes; 
    address = get16();           checksum += (address & 0xFF) + ((address>>8) & 0xFF);
    type    = get8();            checksum += type;
    
    for(i = 0; i < bytes; i++) {
      buffer[i] = get8();        checksum += buffer[i];
    }    
    checksum += get8();
    
    if (checksum != 0) {
      USART_printstring("SUM ERROR!\r\n");
      return;
    }

    if (0) {
    } else if (type == 1) {  // EOF
      USART_printstring("DONE.\r\n");
      return;
    } else if (type == 0) {  // data field
      checksum = 0;
      for(i = 0; i < bytes; i++) {
        program(segment+address+i, buffer[i]); // this should rety a few times and then error out
        waitReady();   
	if ( buffer[i] != read(segment+address+i)) {   
         USART_printstring("\r\nWrite failure address:");
         printAddress(segment+address+i);
         USART_printstring(".\r\n");	 
         checksum++;
       }
	          
      }
      
      if (checksum == 0) USART_printstring("VERIFIED.\r\n");
      
   } else if (type == 2) { // extended address field
     segment = (buffer[1] | (buffer[0] << 8)) << 4;
     USART_printstring("OK.\r\n");
   }
 }

}


int main(void) {
  char command;

  USART_Init( 7 )   ; // 19200 at 14.7456 Mhz  when writing, wait for "OK." before sending next line!
 // USART_Init( 95 ); // 9600 at 14.7456 Mhz

  DDRA  = 0x00;
  DDRB  = 0xFF;
  DDRC  = 0xFF;
  PORTD = 0xFF; // all control signals high
  DDRD  = 0xFF;
  
  Delay2(4);
  
  
  
  while(1) {
    USART_printstring("READY: ");
    command = USART_Receive();
    USART_printstring("\r\n ");
    if (0){
    } else if (command == 'R') { // read      
      DoRead();
    } else if (command == 'E') { // erase
      USART_printstring("Erasing chip...\r\n");
      chipErase();
      waitReady();
      BlankCheck();
    } else if (command == 'B') { // blank check
      USART_printstring("Checking chip...\r\n");
      BlankCheck();
    } else if (command == 'W') { // write
      HexWrite();
    } else if (command == 'I') { // get ID
      USART_printstring("ID code is: ");
      USART_printhex(getID());
      USART_printstring("\r\n");
    } else if (command == '?') {
      USART_printstring(" R   read  eeprom\r\n");
      USART_printstring(" E   erase eeprom\r\n");
      USART_printstring(" B   blank check\r\n");
      USART_printstring(" I   get chip ID\r\n");
      USART_printstring(" W   write ihex file\r\n");
    }  
  }
  
  return 0;
}

