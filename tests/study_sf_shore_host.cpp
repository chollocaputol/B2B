#include "study_sf_shore.h"
#include <cassert>
#include <cstdio>
using namespace StudyShore;
int main(){
 Policy p;assert(p.mode()==Mode::Auto&&!p.allowed()&&!p.known());
 p.observe(true,kLatitude,kLongitude,false);assert(p.allowed()&&p.distance()<0.001);
 const auto pos=[](double m){return kLatitude+m/111194.92664455874;};
 p.observe(true,pos(125),kLongitude,false);assert(p.allowed()&&p.distance()>100);
 p.observe(true,pos(151),kLongitude,false);assert(!p.allowed());
 p.observe(true,pos(125),kLongitude,false);assert(!p.allowed());
 p.observe(true,pos(99),kLongitude,false);assert(p.allowed());
 p.observe(false,kLatitude,kLongitude,false);assert(!p.allowed()&&!p.known());
 p.observe(true,NAN,kLongitude,false);assert(!p.allowed());
 p.observe(true,91,kLongitude,false);assert(!p.allowed());
 p.observe(true,kLatitude,181,false);assert(!p.allowed());
 p.cycle(false);assert(p.mode()==Mode::Auto&&!p.allowed());
 p.cycle(true);assert(p.mode()==Mode::SimShore&&p.allowed());
 p.observe(false,0,0,true);assert(p.allowed()&&!p.known());
 p.cycle(true);assert(p.mode()==Mode::SimOffshore&&!p.allowed());
 p.observe(true,kLatitude,kLongitude,true);assert(p.near()&&!p.allowed());
 p.cycle(true);assert(p.mode()==Mode::Auto&&p.allowed());
 p.cycle(true);p.observe(false,0,0,false);assert(p.mode()==Mode::Auto&&!p.allowed());
 assert(!permitFresh(0,0));assert(permitFresh(100,3099));assert(!permitFresh(100,3100));
 assert(permitFresh(0xfffffff0U,20U));assert(!permitFresh(0xfffffff0U,4000U));
 std::puts("SHORE1 POLICY PASS: GPS distance, hysteresis, invalid fix, simulation confinement, lease wraparound");
}
