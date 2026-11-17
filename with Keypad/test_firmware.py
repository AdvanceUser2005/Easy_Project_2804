#!/usr/bin/env python3
"""Host tests of actual firmware logic with substituted registers/EEPROM/UART.
Not a hardware simulator: electrical timing and sleep must be tested on AVR.
"""
import pathlib,re,subprocess,tempfile,hashlib
root=pathlib.Path(__file__).resolve().parent
src=(root/'LockerKeypad/Firmware.h').read_text()
# Replace only low-level boundaries; retain credential, parser and state logic.
def replace_function(s,name,body):
 m=re.search(r'\b'+name+r'\([^)]*\)\s*\{',s);assert m,name
 start=s.rfind('\n',0,m.start())+1
 depth=1;i=m.end()
 while depth:
  if s[i]=='{':depth+=1
  if s[i]=='}':depth-=1
  i+=1
 return s[:start]+body+s[i:]
src=re.sub(r'#include <avr/[^>]+>','',src)
src=src.replace('#include "Keypad.h"', 'class Keypad {public: bool healthy()const{return true;} bool attach(){return true;} char getKey(){return 0;}};')
src=replace_function(src,'out','void out(char c){output+=c;}')
src=replace_function(src,'eeRead','uint8_t eeRead(uint16_t a){return memory[a];}')
src=replace_function(src,'eeWrite','void eeWrite(uint16_t a,uint8_t v){if(writes++==cut)throw 1;memory[a]=v;}')
src=replace_function(src,'flushTx','void flushTx(){}')
# All registers are passive variables. No electrical claims come from this test.
regs='SREG UCSR0A UDR0 EECR EEAR EEDR ADCSRA TCCR0A TCCR0B OCR0A PORTD PORTB TCCR1A OCR1A TIMSK2 TCCR2B TCCR1B DDRD DDRB DDRC PORTC TIMSK0 ICR1 TCNT1 TCNT2 TCCR2A OCR2A UBRR0H UBRR0L UCSR0B UCSR0C ACSR PRR MCUSR'.split()
bits=sorted(set(re.findall(r'_BV\((\w+)\)',src)) | {'PD2','PD3'})
pre='#include <string>\n#include <cstring>\n#include <cassert>\n#include <cstdio>\n#include <stdint.h>\n'
pre+='std::string output;uint8_t memory[1024];int writes=0,cut=-1;\n'
pre+='uint16_t '+','.join(regs)+';\n'
pre+='enum {'+','.join(b+'='+str(i%8) for i,b in enumerate(bits))+'};\n'
pre+='#define _BV(x) (1U<<(x))\n#define ISR(x) void x()\n#define PSTR(x) (x)\n#define pgm_read_byte(x) (*(x))\n#define SLEEP_MODE_IDLE 0\nvoid cli(){} void sei(){} void set_sleep_mode(int){} void sleep_enable(){} void sleep_disable(){} void sleep_cpu(){}\n'
tests=r'''
void boot(){slot=-1;enrolled=false;credentialFault=false;load();clearChange();failures=0;lockedOut=false;moving=false;lineLen=0;rh=rt=0;rxFault=false;discard=false;afterCR=false;}
void cmd(const char*s){char b[64];strcpy(b,s);command(b);}
void feed(const char*s){while(*s){rx[rh]=*s++;rh=(rh+1)&63;receive();}}
int main(){
 memset(memory,255,sizeof(memory));boot();assert(!enrolled&&!credentialFault);
 cmd("SET abcdef12");cmd("abcdef12");assert(enrolled&&matches("abcdef12")&&!matches("wrong"));
 boot();assert(enrolled&&matches("abcdef12"));
 cmd("wrong1");cmd("wrong2");assert(!lockedOut&&failures==2);cmd("wrong3");assert(lockedOut);
 cmd("abcdef12");assert(lockedOut&&!moving);
 ticks=lockoutAt+LOCKOUT_MS;loop();assert(!lockedOut);
 cmd("CHANGE");cmd("badpass");assert(!changing&&matches("abcdef12"));
 cmd("CHANGE");cmd("abcdef12");cmd("newpass99");cmd("mismatch");assert(matches("abcdef12"));
 cmd("CHANGE");cmd("abcdef12");cmd("newpass99");cmd("newpass99");assert(matches("newpass99")&&!matches("abcdef12"));
 boot();assert(matches("newpass99"));
 assert(!pwOK("STATUS")&&!pwOK("CHANGE")&&!pwOK("123")&&!pwOK("has space"));
 cmd("LOCK");assert(moving);ticks=motionAt+MOVE_MS;motionTick(ticks);assert(!moving&&position==LOCKED);
 feed("CHANGE\r\n");assert(changing);feed("newpass99\r\n");assert(authorizedChange);feed("CANCEL\n");assert(!changing);
 feed("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n");assert(!discard&&lineLen==0);
 rxFault=true;receive();assert(discard);feed("\n");assert(!discard);
 uint8_t original[1024];memcpy(original,memory,sizeof(memory));
 for(int interruption=0;interruption<57;interruption++){
  memcpy(memory,original,sizeof(memory));boot();writes=0;cut=interruption;
  try{savePassword("thirdpass88");}catch(int){}
  cut=-1;boot();assert(enrolled&&!credentialFault&&matches("newpass99"));
 }
 memcpy(memory,original,sizeof(memory));boot();savePassword("thirdpass88");boot();assert(matches("thirdpass88"));
 memset(memory,0,sizeof(memory));boot();assert(credentialFault&&!enrolled);
 puts("PASS: enrollment, persistence, authentication, lockout, password changes, serial parsing, corrupt storage and 57 interrupted EEPROM commits.");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=pathlib.Path(d)/'firmware.cpp';p.write_text(pre+src+tests)
 subprocess.run(['g++','-std=c++11','-w','-I'+str(root/'LockerKeypad'),str(p),'-o',d+'/test'],check=True)
 subprocess.run([d+'/test'],check=True)
 subprocess.run(['g++','-std=c++11','-Wall','-Wextra','-Werror',str(root/'test_sha.cpp'),'-o',d+'/sha'],check=True)
 for n in [0,1,3,55,56,63,64,65,1000]:
  s='abc' if n==3 else 'a'*n
  actual=subprocess.check_output([d+'/sha',s],text=True).strip()
  assert actual==hashlib.sha256(s.encode()).hexdigest(),n
 print('PASS: SHA-256 matches hashlib for nine vectors, including padding boundaries.')


# Test timed servo behavior and isolation from all other PORTB pins.
motor_tests=r'''
int main(){
 memset(memory,255,sizeof(memory));DDRB=0;PORTB=0xff;setup();
 assert(DDRB==_BV(PB1));assert(ADCSRA==0);assert(PRR&_BV(PRADC));
 const uint16_t otherPins=PORTB&~_BV(PB1);
 ticks=1000;startMove(LOCKED);assert(moving);motionTick(1599);assert(moving);motionTick(1600);assert(!moving&&position==LOCKED);
 assert((PORTB&~_BV(PB1))==otherPins);assert(!(PORTB&_BV(PB1)));assert(TCCR1A==0);
 ticks=2000;startMove(UNLOCKED);assert(OCR1A==UNLOCK_US*2);motionTick(2100);assert(moving);motionTick(2600);assert(!moving&&position==UNLOCKED);
 ticks=3000;startMove(LOCKED);startMove(UNLOCKED);assert(target==LOCKED);motionTick(3600);assert(position==LOCKED&&!moving);
 assert((PORTB&~_BV(PB1))==otherPins);
 sleepIdle();assert((PORTB&~_BV(PB1))==otherPins);assert(ADCSRA==0);
 assert(UCSR0B&(_BV(RXEN0)|_BV(RXCIE0)));
 puts("PASS: timed lock/unlock, no supply-control pin, ADC disabled, sleep cleanup and rejection of overlapping moves.");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=pathlib.Path(d)/'motion.cpp';p.write_text(pre+src+motor_tests)
 subprocess.run(['g++','-std=c++11','-w','-I'+str(root/'LockerKeypad'),str(p),'-o',d+'/motion'],check=True)
 subprocess.run([d+'/motion'],check=True)

keypad_tests=tests[:tests.index('int main()')]+r'''
void key(const char*s){while(*s)keypadInput(*s++);}
int main(){
 memset(memory,255,sizeof(memory));boot();clearKeyEntry();
 key("123456#123456#");assert(enrolled&&matches("123456"));
 key("#");assert(moving&&target==LOCKED);ticks=motionAt+MOVE_MS;motionTick(ticks);
 key("123456#");assert(moving&&target==UNLOCKED);ticks=motionAt+MOVE_MS;motionTick(ticks);
 key("*#123456#654321#654321#");assert(matches("654321"));
 key("123*");assert(keyLen==0&&!changing);key("#");assert(changing);key("*");assert(!changing);
 key("111111#111111#111111#");assert(lockedOut);key("654321#");assert(!moving&&keyLen==0);
 ticks=lockoutAt+LOCKOUT_MS;loop();assert(!lockedOut);
 key("1234567890123456789012345#");assert(!moving&&keyLen==0);
 key("654321#");assert(moving);key("654321#");assert(keyLen==0);ticks=motionAt+MOVE_MS;motionTick(ticks);
 key("123");sleepIdle();assert(keyLen==0&&!changing);
 PORTD=_BV(PD2)|_BV(PD3);leds(7);assert((PORTD&(_BV(PD2)|_BV(PD3)))==(_BV(PD2)|_BV(PD3)));assert(PORTC&_BV(PC0));
 puts("PASS: keypad enrollment, confirmation, locking, unlocking, password changes, cancel, lockout, overflow, motion rejection, sleep cleanup and D2/D3 isolation.");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=pathlib.Path(d)/'keypad.cpp';p.write_text(pre+src+keypad_tests)
 subprocess.run(['g++','-std=c++11','-w','-I'+str(root/'LockerKeypad'),str(p),'-o',d+'/keypad'],check=True)
 subprocess.run([d+'/keypad'],check=True)
