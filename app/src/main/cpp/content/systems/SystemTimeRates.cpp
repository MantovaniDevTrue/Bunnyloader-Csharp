#include "content/systems/SystemTimeRates.h"
#include "content/systems/SystemContracts.h"
#include "boot/Boot.h"
#include <atomic>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <mutex>
#include <unistd.h>

namespace bl::runtime::systems {
namespace {
struct NumericField { FieldInfo* field=nullptr; SystemStageFieldKind kind=SystemStageFieldKind::Single; };
NumericField dayField, tileField;
std::mutex prepareMutex;
std::atomic<bool> prepared{false};
unsigned depth=0;
bool published=false, observedMenu=true;
TimeRates current{};
double tileRemainder=0, eventRemainder=0;
std::atomic<double> clock{std::numeric_limits<double>::quiet_NaN()};
bool fail(std::string* error,const char* stage,const char* reason,const std::string& expected,const std::string& actual) {
    if(error) *error="[Native][ERROR][SystemTimeRates] Stage="+std::string(stage)+" Expected="+expected+
        " Actual="+actual+" Result=Rejected Reason="+reason;
    return false;
}
bool thread(std::string* error) {
    const auto expected=gameThreadId();
    return (expected>0 && expected==gettid()) || fail(error,"ThreadCheck","WrongThread",std::to_string(expected),std::to_string(gettid()));
}
double read(const NumericField& field) {
    auto& api=il2cpp::api();
    if(field.kind==SystemStageFieldKind::Double) { double v=0;api.field_static_get_value(field.field,&v);return v; }
    if(field.kind==SystemStageFieldKind::Int32) { int32_t v=0;api.field_static_get_value(field.field,&v);return v; }
    float v=0;api.field_static_get_value(field.field,&v);return v;
}
void write(const NumericField& field,double rate) {
    auto& api=il2cpp::api();
    if(field.kind==SystemStageFieldKind::Double) api.field_static_set_value(field.field,&rate);
    else if(field.kind==SystemStageFieldKind::Int32) { int32_t v=static_cast<int32_t>(rate);api.field_static_set_value(field.field,&v); }
    else {
        float v=static_cast<float>(rate);
        // Int32.MaxValue rounds UP when represented as Single. Do not publish
        // an out-of-contract counter which native loops may convert to Int32.
        if(double(v)>2147483647.0) v=std::nextafter(v,0.0f);
        api.field_static_set_value(field.field,&v);
    }
}
bool valid(TimeRates rates) { return validTimeRate(rates.time) && validTimeRate(rates.tiles) && validTimeRate(rates.events); }
}
bool prepareTimeRates(std::string* error) {
    std::string localError;if(!error) error=&localError;
    if(prepared.load(std::memory_order_acquire)) return true;
    std::lock_guard lock(prepareMutex);
    if(prepared.load(std::memory_order_relaxed)) return true;
    auto* main=il2cpp::findClassQuiet("Terraria","Main");
    NumericField day,tiles;
    day.field=systemStageField(main,"dayRate",true,&day.kind,error,"SystemTimeRates");
    if(!day.field) return false;
    tiles.field=systemStageField(main,"desiredWorldTilesUpdateRate",true,&tiles.kind,error,"SystemTimeRates");
    if(!tiles.field) return false;
    dayField=day;tileField=tiles;
    prepared.store(true,std::memory_order_release);
    return true;
}
bool beginTimeRates(std::string* error) {
    if(!thread(error) || !prepareTimeRates(error)) return false;
    if(depth>=16) return fail(error,"Begin","NestingLimit","Depth<16",std::to_string(depth));
    if(depth++==0) published=false;
    return true;
}
bool readTimeRates(TimeRates* rates,std::string* error) {
    if(!thread(error)) return false;
    if(!rates || !depth) return fail(error,"Read","NoActiveTransaction","ActiveTransactionAndOutput",!rates?"NullOutput":"Depth=0");
    if(!published) {
        const double day=read(dayField);
        current={day,read(tileField),day};
    }
    if(!valid(current)) return fail(error,"Read","InvalidNativeRate","FiniteRates:0..2147483647",
        std::to_string(current.time)+","+std::to_string(current.tiles)+","+std::to_string(current.events));
    *rates=current;return true;
}
bool publishTimeRates(TimeRates rates,std::string* error) {
    if(!thread(error)) return false;
    if(!depth) return fail(error,"Publish","NoActiveTransaction","Depth>0","0");
    if(!valid(rates)) return fail(error,"Publish","InvalidRate","FiniteRates:0..2147483647",
        std::to_string(rates.time)+","+std::to_string(rates.tiles)+","+std::to_string(rates.events));
    current=rates;published=true;return true;
}
bool endTimeRates(std::string* error) {
    if(!thread(error)) return false;
    if(!depth) return fail(error,"End","NoActiveTransaction","Depth>0","0");
    if(--depth) return true;
    if(!published) { clock.store(std::numeric_limits<double>::quiet_NaN(),std::memory_order_release);return true; }
    const double tiles=std::min(2147483647.0,current.tiles+tileRemainder);
    const double events=std::min(2147483647.0,current.events+eventRemainder);
    // Both contracts were validated before publication. Only this outermost
    // commit rounds values; other languages never see another mod's floor().
    write(tileField,std::floor(tiles));write(dayField,std::floor(events));
    tileRemainder=tiles-std::floor(tiles);eventRemainder=events-std::floor(events);
    clock.store(current.time,std::memory_order_release);
    return true;
}
double clockRate() { return clock.load(std::memory_order_acquire); }
void observeTimeRateMenu(bool menu) {
    if(gameThreadId()<=0 || gameThreadId()!=gettid()) return;
    if(menu!=observedMenu) {
        depth=0;published=false;tileRemainder=eventRemainder=0;
        clock.store(std::numeric_limits<double>::quiet_NaN(),std::memory_order_release);
        observedMenu=menu;
    }
}
}
