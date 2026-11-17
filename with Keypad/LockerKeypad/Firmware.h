/* IDP locker, ATmega328P, 16 MHz, 9600 baud. See README before connecting.
 * No Arduino libraries used. AVR device headers are compiler support.
 * Timer0: buzzer; Timer1: servo; Timer2: 1ms clock. No millis()/Serial/tone().
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <stdint.h>
#include <string.h>
#include "Sha256.h"
#include "Keypad.h"
Keypad keypad;

// Basic bench setup: servo signal only; no supply switch or analog sensors.
static const uint16_t LOCK_US=1000, UNLOCK_US=2000; // calibrate mechanism
static const uint16_t MOVE_MS=600; // fixed movement interval; no blockage detection
static const uint32_t IDLE_MS=10000, LOCKOUT_MS=30000;

volatile uint32_t ticks=0;
volatile uint8_t rx[64],rh=0,rt=0;
volatile bool rxFault=false;
volatile uint32_t entropy=0x17a2b39d;
ISR(TIMER2_COMPA_vect){ticks++;}
ISR(USART_RX_vect){uint8_t s=UCSR0A,c=UDR0;uint8_t next=(rh+1)&63;
 entropy=(entropy<<5)^(entropy>>2)^TCNT1^((uint32_t)TCNT2<<16)^c;
 if((s&(_BV(FE0)|_BV(DOR0)|_BV(UPE0)))||next==rt)rxFault=true;
 else{rx[rh]=c;rh=next;}}
uint32_t now(){uint8_t s=SREG;cli();uint32_t t=ticks;SREG=s;return t;}
void wipe(void*p,uint16_t n){volatile uint8_t*q=(volatile uint8_t*)p;while(n--)*q++=0;}
void out(char c){while(!(UCSR0A&_BV(UDRE0))){} UCSR0A|=_BV(TXC0);UDR0=c;}
void say(const char*p){char c;while((c=pgm_read_byte(p++)))out(c);}
#define SAY(s) say(PSTR(s))
void number(uint32_t v){char b[11];uint8_t i=0;do{b[i++]=(char)('0'+v%10);v/=10;}while(v);while(i)out(b[--i]);}
void flushTx(){while(!(UCSR0A&_BV(TXC0))){} }
uint8_t eeRead(uint16_t a){while(EECR&_BV(EEPE)){} EEAR=a;EECR|=_BV(EERE);return EEDR;}
void eeWrite(uint16_t a,uint8_t v){if(eeRead(a)==v)return;while(EECR&_BV(EEPE)){} EEAR=a;EEDR=v;uint8_t s=SREG;cli();EECR|=_BV(EEMPE);EECR|=_BV(EEPE);SREG=s;while(EECR&_BV(EEPE)){} }
uint16_t crc(const uint8_t*p,uint8_t n){uint16_t x=0xffff;while(n--){x^=(uint16_t)*p++<<8;for(uint8_t i=0;i<8;i++)x=(x&0x8000)?(x<<1)^0x1021:x<<1;}return x;}
// Explicit byte layout, two journal slots; last byte is commit marker.
// version=1, seq little-endian [1..4], salt [5..20], hash [21..52], CRC [53..54], commit[55].
uint8_t record[56];int8_t slot=-1;bool enrolled=false,credentialFault=false;
bool valid(uint8_t*r){uint16_t c=crc(r,53);return r[0]==1&&r[55]==0xa5&&r[53]==(uint8_t)c&&r[54]==(uint8_t)(c>>8);}
uint32_t seq(const uint8_t*r){return (uint32_t)r[1]|((uint32_t)r[2]<<8)|((uint32_t)r[3]<<16)|((uint32_t)r[4]<<24);}
void load(){uint8_t other[56];for(uint8_t i=0;i<56;i++){record[i]=eeRead(i);other[i]=eeRead(64+i);}bool a=valid(record),b=valid(other);
 if(b&&(!a||(int32_t)(seq(other)-seq(record))>0)){memcpy(record,other,56);slot=1;}else if(a)slot=0;
 enrolled=slot>=0;bool erased=true;for(uint8_t i=0;i<56;i++)if(record[i]!=255||other[i]!=255)erased=false;credentialFault=!enrolled&&!erased;wipe(other,56);}
void digest(const uint8_t*salt,const char*pw,uint8_t*outHash){Sha256 s;s.add(salt,16);s.add((const uint8_t*)pw,strlen(pw));s.finish(outHash);wipe(&s,sizeof(s));}
void savePassword(const char*pw){uint32_t generation=enrolled?seq(record)+1:1;record[0]=1;for(uint8_t i=0;i<4;i++)record[1+i]=(uint8_t)(generation>>(8*i));
 uint8_t s=SREG;cli();uint32_t e=entropy^ticks^TCNT1;SREG=s;
 // Timer/user timing supplies diversification, not cryptographic entropy.
 for(uint8_t i=0;i<16;i++){e^=e<<13;e^=e>>17;e^=e<<5;e+=TCNT1+generation+i;record[5+i]=(uint8_t)e;}
 digest(record+5,pw,record+21);uint16_t c=crc(record,53);record[53]=c;record[54]=c>>8;record[55]=0xa5;
 uint8_t target=slot==0?1:0;uint16_t base=target*64;eeWrite(base+55,0);for(uint8_t i=0;i<55;i++)eeWrite(base+i,record[i]);eeWrite(base+55,0xa5);slot=target;enrolled=true;}
bool matches(const char*p){uint8_t d[32],diff=0;digest(record+5,p,d);for(uint8_t i=0;i<32;i++)diff|=d[i]^record[21+i];wipe(d,32);return diff==0;}
bool pwOK(const char*p){if(!strcmp(p,"STATUS")||!strcmp(p,"CHANGE")||!strcmp(p,"CANCEL"))return false;uint8_t n=strlen(p);if(n<6||n>24)return false;while(*p){if(*p<33||*p>126)return false;p++;}return true;}
// Feedback overlays: one chirp/key, two/correct, three/wrong, six/lockout.
uint8_t pulses=0,color=0;uint32_t patternAt=0;
void toneOn(bool on){if(on){TCCR0A=_BV(WGM01)|_BV(COM0A0);TCCR0B=_BV(CS01)|_BV(CS00);OCR0A=62;}else{TCCR0A=0;TCCR0B=0;PORTD&=~_BV(PD6);}}
void signalEvent(uint8_t c,uint8_t p){color=c;pulses=p;patternAt=now();}
// color bits: green=1, red=2, amber=4
void leds(uint8_t c){PORTD=(PORTD&~(_BV(PD4)|_BV(PD5)))|((c&2)?_BV(PD4):0)|((c&4)?_BV(PD5):0); if(c&1)PORTC|=_BV(PC0);else PORTC&=~_BV(PC0);}
enum Position:uint8_t{UNKNOWN,LOCKED,UNLOCKED};Position position=UNKNOWN,target=UNKNOWN;
bool moving=false;uint32_t motionAt=0;
uint8_t failures=0;bool lockedOut=false;uint32_t lockoutAt=0;
bool changing=false,authorizedChange=false;char pending[25];
uint32_t lastActivity=0;
// Stop control pulses only. Servo V+ stays connected to the existing 5 V supply.
void servoOff(){TCCR1A=0;PORTB&=~_BV(PB1);}
void servoStart(Position p){OCR1A=(p==LOCKED?LOCK_US:UNLOCK_US)*2;TCCR1A=_BV(COM1A1)|_BV(WGM11);}
void startMove(Position p){
 if(moving){SAY("Busy; retry after motion.\r\n");return;}
 target=p;moving=true;motionAt=now();servoStart(p);
}
void motionTick(uint32_t t){
 if(moving&&t-motionAt>=MOVE_MS){
  servoOff();position=target;moving=false;
  SAY("Movement interval complete (commanded position, no position sensor).\r\n");
  signalEvent(position==LOCKED?4:1,2);lastActivity=t;
 }
}
void feedback(uint32_t t){if(pulses){uint32_t dt=t-patternAt;if(dt>=(uint32_t)pulses*240){pulses=0;toneOn(false);}else{bool on=(dt%240)<100;leds(on?color:0);toneOn(on);return;}}
 toneOn(false);uint8_t c=lockedOut?2:(changing?4:(position==UNLOCKED?1:4));leds(c);}
void badPassword(){failures++;SAY("Incorrect password.\r\n");signalEvent(2,3);if(failures>=3){lockedOut=true;lockoutAt=now();failures=0;signalEvent(2,6);SAY("Three failures: 30-second lockout.\r\n");}}
void clearChange(){changing=false;authorizedChange=false;wipe(pending,sizeof(pending));}
void status(){SAY("State: ");if(moving)SAY("MOVING");else if(position==LOCKED)SAY("LOCKED(commanded)");else if(position==UNLOCKED)SAY("UNLOCKED(commanded)");else SAY("UNKNOWN");
 SAY("; enrolled=");number(enrolled);SAY("\r\n");}
void command(char*line){if(!*line)return;
 if(credentialFault){SAY("EEPROM credentials corrupt: fail closed. Recover physically with programmer.\r\n");return;}
 if(lockedOut){SAY("Locked out; wait.\r\n");return;}
 if(!strcmp(line,"CANCEL")){clearChange();SAY("Cancelled.\r\n");return;}
 if(!strcmp(line,"STATUS")){status();return;}
 if(!strcmp(line,"HELP")){SAY("Enter password to unlock. Commands: LOCK, CHANGE, STATUS, CANCEL, HELP. First boot: SET password then repeat it.\r\n");return;}
 if(moving){SAY("Busy; retry after motion.\r\n");return;}
 if(!strcmp(line,"LOCK")){clearChange();if(!enrolled){SAY("Set password first.\r\n");return;}startMove(LOCKED);return;}
 if(!enrolled){if(!strncmp(line,"SET ",4)&&pwOK(line+4)){strcpy(pending,line+4);changing=true;SAY("Repeat new password to confirm.\r\n");return;}
  if(changing&&pwOK(line)&&!strcmp(line,pending)){savePassword(line);clearChange();SAY("Password stored. Use LOCK to establish commanded lock position.\r\n");signalEvent(1,2);}
  else{clearChange();SAY("First boot: SET followed by a 6-24 character password.\r\n");}return;}
 if(!strcmp(line,"CHANGE")){clearChange();changing=true;SAY("Enter CURRENT password.\r\n");return;}
 if(changing){if(!authorizedChange){if(matches(line)){authorizedChange=true;failures=0;SAY("Enter NEW 6-24 character password.\r\n");signalEvent(4,2);}else{clearChange();badPassword();}return;}
  if(!pending[0]){if(!pwOK(line)){SAY("Use 6-24 printable non-space characters.\r\n");return;}strcpy(pending,line);SAY("Repeat NEW password.\r\n");return;}
  if(!strcmp(line,pending)){savePassword(line);SAY("Password updated.\r\n");signalEvent(1,2);}else{SAY("Confirmation mismatch; unchanged.\r\n");signalEvent(2,3);}clearChange();return;}
 if(matches(line)){failures=0;SAY("Correct password.\r\n");signalEvent(1,2);startMove(UNLOCKED);}else badPassword();}
char lineBuf[40];uint8_t lineLen=0;bool discard=false,afterCR=false;
void receive(){if(rxFault){uint8_t s=SREG;cli();rt=rh;rxFault=false;SREG=s;wipe(lineBuf,sizeof(lineBuf));lineLen=0;discard=true;clearChange();SAY("UART error/overflow; line rejected. Send newline, then retry.\r\n");}
 while(rt!=rh){char c=rx[rt];rt=(rt+1)&63;lastActivity=now();
 if(c=='\n'&&afterCR){afterCR=false;continue;}afterCR=c=='\r';
 if(c=='\r'||c=='\n'){if(!discard){lineBuf[lineLen]=0;command(lineBuf);}else SAY("Input rejected.\r\n");wipe(lineBuf,sizeof(lineBuf));lineLen=0;discard=false;continue;}
 if(discard)continue;if(c==8||c==127){if(lineLen)lineBuf[--lineLen]=0;continue;}
 if(c<32||c>126||lineLen>=sizeof(lineBuf)-1){discard=true;continue;}lineBuf[lineLen++]=c;signalEvent(4,1);
 }}
// TCA8418 INT was not wired in the original design. Keep Timer2 running so
// IDLE wakes every ms and the main loop can poll the keypad every 5 ms.
// This deliberately trades some sleep current for requiring no extra wire.
void clearKeyEntry();
bool idleAnnounced=false;
void sleepIdle(){
 if(!idleAnnounced){clearKeyEntry();clearChange();wipe(lineBuf,sizeof(lineBuf));lineLen=0;
  SAY("IDLE; keypad polling remains active.\r\n");flushTx();idleAnnounced=true;}
 set_sleep_mode(SLEEP_MODE_IDLE);cli();
 if(rt==rh&&!rxFault){sleep_enable();sei();sleep_cpu();sleep_disable();}else sei();
}
char keyBuf[25];uint8_t keyLen=0;bool keyOverflow=false,starPrefix=false;
void clearKeyEntry(){wipe(keyBuf,sizeof(keyBuf));keyLen=0;keyOverflow=false;starPrefix=false;}
void keypadInput(char key){
 lastActivity=now();
 entropy=(entropy<<5)^(entropy>>2)^lastActivity^TCNT1^(uint8_t)key;
 if(lockedOut||moving||credentialFault){clearKeyEntry();return;}
 if(key=='*'){clearKeyEntry();clearChange();starPrefix=true;signalEvent(4,1);SAY("Entry cleared. *# starts password change.\r\n");return;}
 if(key=='#'){
  if(starPrefix){clearKeyEntry();if(enrolled){char c[]="CHANGE";command(c);}return;}
  if(keyOverflow){clearKeyEntry();clearChange();SAY("PIN too long; entry rejected.\r\n");return;}
  if(!keyLen){if(enrolled&&!changing){char c[]="LOCK";command(c);}return;}
  keyBuf[keyLen]=0;
  if(!enrolled&&!changing){char c[29]="SET ";strcpy(c+4,keyBuf);command(c);wipe(c,sizeof(c));}
  else command(keyBuf);
  clearKeyEntry();return;
 }
 if(key>='0'&&key<='9'){
  starPrefix=false;
  if(keyLen<24&&!keyOverflow)keyBuf[keyLen++]=key;else keyOverflow=true;
  signalEvent(4,1);
 }
}
uint32_t keypadAt=0,retryAt=0;
void keypadTick(uint32_t t){
 if(!keypad.healthy()){
  if(t-retryAt>=1000){retryAt=t;if(keypad.attach())SAY("Keypad connected; entry cleared.\r\n");}
  return;
 }
 if(t-keypadAt<5)return;keypadAt=t;
 char key=keypad.getKey();
 if(!keypad.healthy()){
  clearKeyEntry();clearChange();SAY("Keypad bus/FIFO fault; entry rejected. Retrying.\r\n");retryAt=t;return;
 }
 if(key)keypadInput(key);
}
void setup(){cli();MCUSR=0; // Bootloader must leave watchdog disabled.
 DDRD|=_BV(PD4)|_BV(PD5)|_BV(PD6);DDRC|=_BV(PC0);DDRB|=_BV(PB1);servoOff();
 TIMSK0=0;TCCR0A=0;TCCR0B=0;
 TCCR1A=0;TCCR1B=_BV(WGM13)|_BV(WGM12)|_BV(CS11);ICR1=39999;TCNT1=0;
 TCCR2A=_BV(WGM21);TCCR2B=_BV(CS22);OCR2A=249;TIMSK2=_BV(OCIE2A);
 UBRR0H=0;UBRR0L=103;UCSR0A=0;UCSR0B=_BV(RXEN0)|_BV(TXEN0)|_BV(RXCIE0);UCSR0C=_BV(UCSZ01)|_BV(UCSZ00);
 ADCSRA=0;ACSR=_BV(ACD);PRR=_BV(PRADC)|_BV(PRSPI)|_BV(PRTWI);sei();
 load();if(!keypad.attach())SAY("Keypad missing; check D2/D3 and pull-ups.\r\n");SAY("\r\nIDP TCA8418 locker, 9600 baud. Digits then #; empty # locks; * clears; *# changes PIN.\r\n");
 if(credentialFault)SAY("EEPROM fault; physical recovery required.\r\n");else if(!enrolled)SAY("No valid credential. Physically secure device; use SET password.\r\n");lastActivity=now();}
void loop(){receive();uint32_t t=now();keypadTick(t);if(lockedOut&&t-lockoutAt>=LOCKOUT_MS){lockedOut=false;SAY("Lockout ended.\r\n");lastActivity=t;}
 motionTick(t);feedback(t);
 if(idleAnnounced&&t-lastActivity<IDLE_MS){idleAnnounced=false;SAY("Awake.\r\n");}
 if(!moving&&!lockedOut&&!pulses&&t-lastActivity>=IDLE_MS)sleepIdle();}
