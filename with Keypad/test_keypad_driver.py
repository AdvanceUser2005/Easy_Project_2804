#!/usr/bin/env python3
"""Tests the actual FIFO consumer with mocked I2C register boundaries."""
import pathlib,re,subprocess,tempfile
root=pathlib.Path(__file__).resolve().parent
src=(root/'LockerKeypad/Keypad.cpp').read_text()
def replace(s,name,body):
 m=re.search(r'bool Keypad::'+name+r'\([^)]*\)\s*\{',s);assert m
 depth=1;i=m.end()
 while depth:
  if s[i]=='{':depth+=1
  if s[i]=='}':depth-=1
  i+=1
 return s[:m.start()]+body+s[i:]
src=replace(src,'readRegister', '''bool Keypad::readRegister(uint8_t r,uint8_t& v){
 if(failBus)return false;
 if(r==2)v=overflow?8:0;
 else if(r==3)v=events.size();
 else if(r==4){v=events.empty()?0:events.front();if(!events.empty())events.pop_front();}
 else v=0;
 return true;
}''')
src=replace(src,'writeRegister','bool Keypad::writeRegister(uint8_t,uint8_t){return !failBus;}')
# Expose only private state to prime initialization; do not alter FIFO/decoder logic.
pre='''#include <deque>
#include <cassert>
#include <cstdio>
#include <stdint.h>
std::deque<uint8_t> events;bool failBus=false,overflow=false;
#define private public
#include "Keypad.h"
#undef private
'''
test=r'''
int main(){
 Keypad k;k._initialized=true;
 assert(k.getKey()==0);
 const char* keys="123456789*0#";
 for(int i=0;i<12;i++){
  uint8_t event=(i/3)*10+(i%3)+1;
  events.push_back(event|0x80);events.push_back(event);
  assert(k.getKey()==keys[i]);assert(k.getKey()==0);
 }
 // Both press and release queued before host polls: quick taps must survive.
 events={0x81,1,0x81,1,0x82,2};
 assert(k.getKey()=='1');assert(k.getKey()=='1');assert(k.getKey()=='2');assert(!k.getKey());
 events={0x84,0xE1,0x80};assert(!k.getKey()); // unused column, GPI and empty code
 overflow=true;assert(!k.getKey()&&!k.healthy());overflow=false;k._initialized=true;
 failBus=true;assert(!k.getKey()&&!k.healthy());failBus=false;
 k._initialized=true;events.assign(11,0x81);assert(!k.getKey()&&!k.healthy());
 puts("PASS: all 12 keys, press/release polarity, quick/repeated taps, FIFO ordering, unused events, overflow and bus failure.");
}
'''
platform='''#pragma once
#include <stdint.h>
static const uint8_t LOW=0,HIGH=1,INPUT=0,OUTPUT=1;
inline void digitalWrite(uint8_t,uint8_t){}
inline void pinMode(uint8_t,uint8_t){}
inline uint8_t digitalRead(uint8_t){return 1;}
inline uint32_t millis(){return 0;}
inline uint32_t micros(){static uint32_t t=0;return t++;}
inline void delayMicroseconds(unsigned int){}
'''
with tempfile.TemporaryDirectory() as d:
 p=pathlib.Path(d);(p/'KeypadTestPlatform.h').write_text(platform)
 (p/'test.cpp').write_text(pre+src+test)
 subprocess.run(['g++','-std=c++11','-DKEYPAD_HOST_TEST','-I'+str(p),'-I'+str(root/'LockerKeypad'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
