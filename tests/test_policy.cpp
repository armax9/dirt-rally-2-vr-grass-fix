#include <cstdio>
#include <cassert>
#include "../far_policy.h"
int main(){
 using namespace grassFarPolicy;
 auto p=plan(320,1280,78336,19584,6*4096);
 assert(p.capacity==1740800&&p.changed&&!p.capped&&p.totalBytes==199915520);
 assert(plan(832,1024,78336,19584,24576).capacity==1740800);
 assert(plan(0,64,78336,19584,24576).capacity==78336);
 assert(plan(16385,64,78336,19584,24576).capacity==78336);
 auto capped=plan(16384,16384,78336,19584,24576);assert(capped.capped&&capped.totalBytes<=byteLimit&&capped.capacity>=78336);
 assert(plan(64,64,10000,19584,24576).capacity==10000);
 assert(plan(320,1280,78336,19584,UINT64_MAX).capacity==78336);
 unsigned tests=7;
 for(unsigned a=64;a<=2048;a+=64)for(unsigned b=64;b<=2048;b+=64){auto x=plan(a,b,78336,19584,24576);assert(x.capacity>=78336&&x.totalBytes<=byteLimit);assert(x.totalBytes==(uint64_t(x.capacity)+19584+24576)*112);++tests;}
 printf("PASS %u capacity-policy checks; near/custom reserves unchanged; sample total 199915520 bytes (190.65 MiB)\n",tests);
}
