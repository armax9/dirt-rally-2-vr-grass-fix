// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once
#include <stdint.h>
namespace grassFarPolicy {
static constexpr uint64_t stride=112,byteLimit=256ull*1024*1024;
struct Plan{uint32_t capacity=0;uint64_t gridCells=0,totalBytes=0;bool changed=false,capped=false;const char*status="unchanged";};
static Plan plan(uint32_t dimension0,uint32_t dimension1,uint32_t originalFar,uint32_t nearCapacity,uint64_t otherCapacity){
 Plan p;p.capacity=originalFar;
 if(!dimension0||!dimension1||dimension0>16384||dimension1>16384){p.status="invalid-grid";return p;}
 if(otherCapacity>byteLimit/stride){p.status="material-budget-exceeds-safety-limit";return p;}
 uint64_t fixed=uint64_t(nearCapacity)+otherCapacity;
 if(fixed>byteLimit/stride||uint64_t(originalFar)>byteLimit/stride-fixed){p.status="existing-budget-exceeds-safety-limit";return p;}
 p.gridCells=uint64_t(dimension0)*dimension0+uint64_t(dimension1)*dimension1;
 uint64_t available=byteLimit/stride-fixed;
 uint64_t desired=p.gridCells;if(desired>available){desired=available;p.capped=true;}
 if(desired<originalFar)desired=originalFar;
 p.capacity=uint32_t(desired);p.totalBytes=(fixed+desired)*stride;p.changed=p.capacity!=originalFar;
 p.status=p.capped?"full-grid-reserve-memory-capped":p.changed?"full-grid-reserve":"existing-reserve-sufficient";
 return p;
}
}
