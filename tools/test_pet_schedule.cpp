#include "../home_pet_schedule.h"
constexpr bool everyMinuteMatches() {
  for (int m = 0; m < 1440; ++m) {
    const bool sleep = m < 420 || m >= 1320 || (m >= 780 && m < 840);
    const bool eat = (m >= 480 && m < 490) || (m >= 1080 && m < 1110);
    const bool leash = m >= 1050 && m < 1080;
    const PetAction expected = sleep ? PetAction::Sleep : eat ? PetAction::Eat :
                               leash ? PetAction::Leash : PetAction::Run;
    if (scheduledPetAction(m, PetAction::Run) != expected) return false;
  }
  return scheduledPetAction(-1, PetAction::Walk) == PetAction::Walk;
}
static_assert(everyMinuteMatches(), "Pet schedule must match all 1440 minutes");

#include <cassert>
#include "pet_schedule_reference.inc"
static bool same(const PetPeriod &a,const PetPeriod &b) {
  return a.start==b.start&&a.end==b.end&&a.action==b.action&&a.kind==b.kind;
}
int main() {
  PetDaySchedule plan,again;
  assert(!buildPetDaySchedule({2026,2,30},plan));
  assert(petDateValid({2024,2,29})&&!petDateValid({2100,2,29}));
  assert(petBathStart({2026,11,22})==949&&petBathStart({2026,11,23})==-1);
  assert(buildPetDaySchedule({2026,11,22},plan));
  assert(plan.count==sizeof(kPetPreviewReference)/sizeof(kPetPreviewReference[0]));
  for(unsigned i=0;i<plan.count;i++)assert(same(plan.periods[i],kPetPreviewReference[i]));
  assert(petBathPhase(299)==PetAction::BathPrepare&&petBathPhase(300)==PetAction::BathScrub);
  assert(petBathPhase(1499)==PetAction::BathScrub&&petBathPhase(1500)==PetAction::BathRinse);
  assert(petBathPhase(2399)==PetAction::BathRinse&&petBathPhase(2400)==PetAction::BathDry);
  assert(petBathPhase(3299)==PetAction::BathDry&&petBathPhase(3300)==PetAction::BathShake);
  unsigned days=0,maxPeriods=0;
  for(int month=1;month<=12;month++)for(int day=1;day<=31;day++){
    const PetDate date{2026,month,day};if(!petDateValid(date))continue;days++;
    assert(buildPetDaySchedule(date,plan)&&buildPetDaySchedule(date,again));
    assert(plan.count&&plan.count==again.count&&plan.periods[0].start==0&&plan.periods[plan.count-1].end==86400);
    maxPeriods=std::max(maxPeriods,(unsigned)plan.count);
    unsigned girls=0,freeSeconds=0,basicSeconds=0,lastGirlEnd=0,spinSeconds=0;
    for(unsigned i=0;i<plan.count;i++){
      const auto &p=plan.periods[i];assert(same(p,again.periods[i]));assert(p.end>p.start);
      if(i)assert(p.start==plan.periods[i-1].end);
      if(p.kind!=PetPeriodKind::Fixed&&p.kind!=PetPeriodKind::Visit)freeSeconds+=p.end-p.start;
      if(p.kind==PetPeriodKind::Visit){
        assert(p.action==PetAction::VisitPlay&&p.end-p.start==7200);
        assert(p.start>=840*60&&p.end<=1050*60);
      }
      if(p.kind==PetPeriodKind::Base){
        assert(p.action==PetAction::Lie||p.action==PetAction::Sit||p.action==PetAction::Walk||p.action==PetAction::Ball);
        basicSeconds+=p.end-p.start;
      }
      if(p.kind==PetPeriodKind::GirlAccent){assert(p.action==PetAction::Spin);spinSeconds=p.end-p.start;}
      if(p.kind==PetPeriodKind::Girl){
        const unsigned start=p.start-spinSeconds,duration=p.end-start;
        assert(duration>=180&&duration<=480);if(girls)assert(start-lastGirlEnd>=5400);
        girls++;lastGirlEnd=p.end;spinSeconds=0;
      }
    }
    assert(girls>=2&&girls<=4&&basicSeconds*100>=freeSeconds*95);
    const int bath=petBathStart(date);if(bath>=0)assert(bath>=840&&bath<=960);
    for(int minute=0;minute<1440;minute++){
      const auto *p=petPeriodAt(plan,minute*60);assert(p);
      const auto fixed=scheduledPetAction(minute,PetAction::Lie);
      if(fixed!=PetAction::Lie)assert(p->action==fixed);
      else if(bath>=0&&minute>=bath&&minute<bath+60)assert(p->action==PetAction::BathPrepare);
    }
  }
  assert(days==365);
  unsigned saturdays=0,sundays=0;
  for(int year=2024;year<=2032;year++)for(int month=1;month<=12;month++)for(int day=1;day<=31;day++){
    const PetDate d{year,month,day};if(!petDateValid(d))continue;
    const int start=petVisitStart(d),weekday=petWeekday(d);
    if(weekday==6){
      const PetDate sunday=petAdjacentDate(d,1);
      assert((start>=0)+(petVisitStart(sunday)>=0)==1);
      assert(petAdjacentDate(sunday,-1).day==d.day);
      if(start>=0)saturdays++;
    }
    if(start>=0){
      assert(weekday==0||weekday==6);assert(buildPetDaySchedule(d,plan));
      unsigned visits=0;for(unsigned i=0;i<plan.count;i++)if(plan.periods[i].kind==PetPeriodKind::Visit)visits++;
      assert(visits==1);
      const int bath=petBathStart(d);
      if(weekday==0){sundays++;assert(start+120<=bath||start>=bath+60);}
    }
  }
  assert(saturdays&&sundays);
  assert(petVisitPhase(23)==PetAction::VisitArrive&&petVisitPhase(24)==PetAction::VisitPlay);
  assert(petVisitPhase(204)==PetAction::VisitRest&&petVisitPhase(7176)==PetAction::VisitLeave);
  std::printf("PASS: approved preview parity, %u daily plans, all fixed-minute boundaries, maximum %u periods\n",days,maxPeriods);
  std::printf("PASS: 9 years of one-visit-per-weekend boundaries, %u Saturdays / %u Sundays, bath preserved\n",saturdays,sundays);
}
