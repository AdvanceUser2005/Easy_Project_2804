#include "LockerKeypad/Sha256.h"
#include <cstdio>
#include <cstring>
int main(int argc,char**argv){Sha256 s;const char*p=argc>1?argv[1]:"";s.add((const uint8_t*)p,strlen(p));uint8_t d[32];s.finish(d);for(auto c:d)printf("%02x",c);puts("");}
