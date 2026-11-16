#pragma once
#include "home_pet_data.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

// End times are exclusive. An unknown clock never starts a scheduled event.
static constexpr PetAction scheduledPetAction(int minute, PetAction daytime) {
  if (minute < 0) return daytime;
  if (minute >= 1320 || minute < 420 || (minute >= 780 && minute < 840)) return PetAction::Sleep;
  if ((minute >= 480 && minute < 490) || (minute >= 1080 && minute < 1110)) return PetAction::Eat;
  if (minute >= 1050 && minute < 1080) return PetAction::Leash;
  return daytime;
}

struct PetDate { int year, month, day; };
enum class PetPeriodKind : uint8_t { Fixed, Base, Girl, Accent, GirlAccent };
struct PetPeriod {
  uint32_t start, end; // seconds since local midnight (minutes only inside builder)
  PetAction action;
  PetPeriodKind kind;
};
static constexpr unsigned kPetMaxPeriods=128;
struct PetDaySchedule { uint16_t count=0; PetPeriod periods[kPetMaxPeriods]; };
static bool petDateValid(PetDate d) {
  static constexpr int days[]={31,28,31,30,31,30,31,31,30,31,30,31};
  if(d.year<2024||d.year>9999||d.month<1||d.month>12||d.day<1)return false;
  const bool leap=d.year%4==0&&(d.year%100!=0||d.year%400==0);
  return d.day<=days[d.month-1]+(d.month==2&&leap);
}
static int petWeekday(PetDate d) {
  static constexpr int offsets[]={0,3,2,5,0,3,5,1,4,6,2,4};
  int y=d.year-(d.month<3);
  return (y+y/4-y/100+y/400+offsets[d.month-1]+d.day)%7;
}
static uint32_t petDateHash(const char *prefix,PetDate d) {
  char text[64];snprintf(text,sizeof(text),"%s%04d-%02d-%02d",prefix,d.year,d.month,d.day);
  uint32_t hash=2166136261U;
  for(const unsigned char *p=(const unsigned char *)text;*p;++p)hash=(hash^*p)*16777619U;
  return hash;
}
static int petBathStart(PetDate d) {
  return petDateValid(d)&&petWeekday(d)==0 ? 840+petDateHash("DeskDock-demo-7893|",d)%121 : -1;
}
struct PetRandom {
  uint32_t seed;
  uint32_t next(uint32_t n) {
    seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;
    return n?seed%n:0;
  }
};
static bool buildPetDaySchedule(PetDate date,PetDaySchedule &out) {
  out.count=0;if(!petDateValid(date))return false;
  PetRandom rng{petDateHash("DeskDock-daily-7893|",date)};
  if(!rng.seed)rng.seed=0x6d2b79f5;
  PetPeriod fixed[7]={{0,420,PetAction::Sleep,PetPeriodKind::Fixed},
    {480,490,PetAction::Eat,PetPeriodKind::Fixed},{780,840,PetAction::Sleep,PetPeriodKind::Fixed},
    {1050,1080,PetAction::Leash,PetPeriodKind::Fixed},{1080,1110,PetAction::Eat,PetPeriodKind::Fixed},
    {1320,1440,PetAction::Sleep,PetPeriodKind::Fixed}};
  unsigned fixedCount=6;
  const int bath=petBathStart(date);
  if(bath>=0)fixed[fixedCount++]={(uint32_t)bath,(uint32_t)bath+60,PetAction::BathPrepare,PetPeriodKind::Fixed};
  auto order=[](const PetPeriod &a,const PetPeriod &b){return a.start<b.start;};
  std::sort(fixed,fixed+fixedCount,order);
  struct Window { uint32_t start,end; };
  Window free[7],eligible[7];unsigned freeCount=0,eligibleCount=0;uint32_t at=0;
  for(unsigned i=0;i<fixedCount;i++){
    if(at<fixed[i].start){
      free[freeCount++]={at,fixed[i].start};
      if(fixed[i].start-at>=18)eligible[eligibleCount++]={at,fixed[i].start};
    }at=fixed[i].end;
  }
  PetPeriod girls[4];unsigned girlCount=0,target=2+rng.next(3);
  for(unsigned attempt=0;girlCount<target&&attempt<256;attempt++){
    const uint32_t duration=3+rng.next(6);uint32_t choices=0;
    for(unsigned i=0;i<eligibleCount;i++)choices+=eligible[i].end-eligible[i].start-duration-9;
    if(!choices)break;
    uint32_t choice=rng.next(choices);unsigned window=0;
    while(choice>=eligible[window].end-eligible[window].start-duration-9){
      choice-=eligible[window].end-eligible[window].start-duration-9;window++;
    }
    const uint32_t start=eligible[window].start+5+choice,end=start+duration;
    bool overlap=false;
    for(unsigned i=0;i<girlCount;i++)if(!(end+90<=girls[i].start||start>=girls[i].end+90))overlap=true;
    if(overlap)continue;
    const uint32_t pick=rng.next(10);
    const PetAction action=pick<6?PetAction::Pet:pick<8?PetAction::Fetch:pick==8?PetAction::Frisbee:PetAction::Tug;
    girls[girlCount++]={start,end,action,PetPeriodKind::Girl};
  }
  if(girlCount<2)return false;
  std::sort(girls,girls+girlCount,order);
  PetPeriod raw[kPetMaxPeriods];unsigned count=0;
  auto append=[&](PetPeriod p){if(count>=kPetMaxPeriods)return false;raw[count++]=p;return true;};
  for(unsigned i=0;i<fixedCount;i++)if(!append(fixed[i]))return false;
  struct Basic { PetAction action;uint32_t weight,low,high; };
  static constexpr Basic basic[]={{PetAction::Lie,40,20,60},{PetAction::Sit,35,10,35},
    {PetAction::Walk,15,3,8},{PetAction::Ball,10,3,10}};
  PetAction previous=PetAction::Sleep;
  auto fill=[&](uint32_t start,uint32_t end){
    while(start<end){
      uint32_t total=0;for(const auto &b:basic)if(b.action!=previous)total+=b.weight;
      uint32_t pick=rng.next(total);const Basic *chosen=nullptr;
      for(const auto &b:basic)if(b.action!=previous){if(pick<b.weight){chosen=&b;break;}pick-=b.weight;}
      if(!chosen)return false;
      const uint32_t next=std::min(end,start+chosen->low+rng.next(chosen->high-chosen->low+1));
      if(!append({start,next,chosen->action,PetPeriodKind::Base}))return false;
      start=next;previous=chosen->action;
    }return true;
  };
  for(unsigned i=0;i<freeCount;i++){
    uint32_t cursor=free[i].start;
    for(unsigned g=0;g<girlCount;g++)if(girls[g].start>=free[i].start&&girls[g].end<=free[i].end){
      if(!fill(cursor,girls[g].start)||!append(girls[g]))return false;cursor=girls[g].end;
    }
    if(!fill(cursor,free[i].end))return false;
  }
  std::sort(raw,raw+count,order);bool spinAdded=false;
  for(unsigned i=0;i<count;i++){
    PetPeriod p=raw[i];p.start*=60;p.end*=60;
    const bool stretch=p.kind==PetPeriodKind::Base&&(p.start==420*60||p.start==840*60);
    const bool spin=p.kind==PetPeriodKind::Girl&&!spinAdded&&(p.action==PetAction::Fetch||p.action==PetAction::Frisbee);
    if(stretch||spin){
      if(out.count>=kPetMaxPeriods-1){out.count=0;return false;}
      out.periods[out.count++]={p.start,p.start+8,stretch?PetAction::Stretch:PetAction::Spin,
        stretch?PetPeriodKind::Accent:PetPeriodKind::GirlAccent};
      p.start+=8;if(spin)spinAdded=true;
    }
    if(out.count>=kPetMaxPeriods){out.count=0;return false;}out.periods[out.count++]=p;
  }
  return true;
}
static const PetPeriod *petPeriodAt(const PetDaySchedule &plan,uint32_t seconds) {
  for(unsigned i=0;i<plan.count;i++)if(seconds>=plan.periods[i].start&&seconds<plan.periods[i].end)return &plan.periods[i];
  return nullptr;
}
static PetAction petBathPhase(uint32_t elapsedSeconds) {
  return elapsedSeconds<300?PetAction::BathPrepare:elapsedSeconds<1500?PetAction::BathScrub:
    elapsedSeconds<2400?PetAction::BathRinse:elapsedSeconds<3300?PetAction::BathDry:PetAction::BathShake;
}
