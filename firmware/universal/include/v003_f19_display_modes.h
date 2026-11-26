#pragma once
// F19R1: Vessel-003 OLED presentation policy ONLY. No SOS, radio, SD or GPIO.
#include <stdint.h>
#include <math.h>
namespace V003F19Display {
enum class Mode : uint8_t { Operational=0, Development=1 };
enum class HomeChoice : uint8_t { ScreenOff=0, Development=1, Portrait=2, Landscape=3, Back=4, None=5 };
enum class Orientation : uint8_t { Landscape=0, Portrait=1 };
static constexpr uint8_t kHomeChoiceCount=5U;
static constexpr uint8_t kOperationalPageCount=4;
#if THESIS_STUDY_SF_CLOUD == 1
static constexpr uint8_t kDevelopmentPageCount=9;
#else
static constexpr uint8_t kDevelopmentPageCount=8;
#endif
static constexpr uint32_t kShortTapMaxMs=900U;
static constexpr uint32_t kModeToggleMinMs=2000U;
inline bool emergencyActive(bool localActive, unsigned receivedActive) {
    return localActive || receivedActive != 0U;
}
// True-north cardinal; NOT vessel-relative heading/compass pointing.
inline const char* cardinal8(double degrees) {
    if (!isfinite(degrees) || degrees < 0.0 || degrees >= 360.0) return "--";
    static const char* const names[8]={"N","NE","E","SE","S","SW","W","NW"};
    unsigned sector = static_cast<unsigned>((degrees + 22.5) / 45.0) % 8U;
    return names[sector];
}
// F21R4.2: view-only emergency navigation. Does NOT own distress state,
// transmission, acknowledgment, cancellation, GPIO, or storage.
class SOSBrowse {
    bool emergency_=false;
    bool browsing_=false;
    uint8_t page_=3U;
    uint32_t lastInputMs_=0U;
public:
    static constexpr uint32_t kReturnToSOSAfterMs=20000U;
    // Returns true when a view state changes (including inactivity timeout).
    bool observe(bool emergency, uint32_t now) {
        if (!emergency) {
            const bool changed=emergency_ || browsing_;
            emergency_=false; browsing_=false; page_=3U;
            return changed;
        }
        if (!emergency_) {
            emergency_=true; browsing_=false; page_=3U;
            lastInputMs_=now;
            return true;
        }
        if (browsing_ && uint32_t(now-lastInputMs_)>=kReturnToSOSAfterMs) {
            browsing_=false; page_=3U;
            return true;
        }
        return false;
    }
    // A newly received event always calls attention to the SOS screen.
    void preempt() { browsing_=false; page_=3U; }
    bool emergency() const { return emergency_; }
    bool browsing() const { return browsing_; }
    bool forceSOS() const { return emergency_ && !browsing_; }
    bool idleForReturn(uint32_t now) const {
        return emergency_ && uint32_t(now-lastInputMs_)>=kReturnToSOSAfterMs;
    }
    uint8_t page() const { return page_; }
    void noteInput(uint32_t now) { lastInputMs_=now; }
    bool tap(uint32_t now) {
        if (!emergency_) return false;
        page_=browsing_ ? uint8_t((page_+1U)%kOperationalPageCount) : 0U;
        browsing_=true;
        lastInputMs_=now;
        return true;
    }
    void returnToSOS(uint32_t now) {
        browsing_=false; page_=3U; lastInputMs_=now;
    }
};
class State {
    Mode mode_=Mode::Operational;
    Orientation orientation_=Orientation::Landscape; // volatile, known-good startup
    uint8_t operationalPage_=0;
    bool homeMenuOpen_=false;
    uint8_t homeMenuCursor_=0;
    bool asleep_=false;
public:
    Mode mode() const { return mode_; }
    Orientation orientation() const { return orientation_; }
    bool portrait() const { return orientation_==Orientation::Portrait; }
    bool operational() const { return mode_==Mode::Operational; }
    uint8_t operationalPage() const { return operationalPage_; }
    bool homeMenuOpen() const { return homeMenuOpen_; }
    void dismissHomeMenuForSOS() { homeMenuOpen_=false; }
    // Emergency display-only menu: independent of the background OP/DEV
    // page and OLED sleep policy, but cannot turn off the SOS display or
    // enter development mode. Emergency status is owned elsewhere.
    bool openHomeMenuForSOS(uint32_t heldMs) {
        if (homeMenuOpen_ || heldMs < kShortTapMaxMs) return false;
        homeMenuOpen_=true; homeMenuCursor_=0; return true;
    }
    bool moveHomeCursorForSOS(uint32_t heldMs) {
        if (!homeMenuOpen_ || heldMs >= kShortTapMaxMs) return false;
        homeMenuCursor_=static_cast<uint8_t>((homeMenuCursor_+1U)%kHomeChoiceCount);
        return true;
    }
    HomeChoice confirmHomeMenuForSOS(uint32_t heldMs) {
        if (!homeMenuOpen_ || heldMs < kShortTapMaxMs) return HomeChoice::None;
        const HomeChoice choice=static_cast<HomeChoice>(homeMenuCursor_);
        if (choice==HomeChoice::ScreenOff || choice==HomeChoice::Development)
            return HomeChoice::None; // locked while SOS remains active
        homeMenuOpen_=false;
        if (choice==HomeChoice::Portrait) orientation_=Orientation::Portrait;
        if (choice==HomeChoice::Landscape) orientation_=Orientation::Landscape;
        return choice;
    }
    uint8_t homeMenuCursor() const { return homeMenuCursor_; }
    bool screenAsleep() const { return asleep_; }
    bool wake() { if (!asleep_) return false; asleep_=false; return true; }
    bool openHomeMenu(uint32_t heldMs, bool sosActive) {
        if (sosActive || asleep_ || !operational() || operationalPage_!=0 ||
            homeMenuOpen_ || heldMs < kShortTapMaxMs) return false;
        homeMenuOpen_=true; homeMenuCursor_=0; return true;
    }
    bool moveHomeCursor(uint32_t heldMs, bool sosActive) {
        if (sosActive || asleep_ || !homeMenuOpen_ || heldMs >= kShortTapMaxMs) return false;
        homeMenuCursor_=static_cast<uint8_t>((homeMenuCursor_+1U)%kHomeChoiceCount);return true;
    }
    HomeChoice confirmHomeMenu(uint32_t heldMs, bool sosActive) {
        if (sosActive || asleep_ || !homeMenuOpen_ || heldMs < kShortTapMaxMs)
            return HomeChoice::None;
        homeMenuOpen_=false;
        const HomeChoice choice=static_cast<HomeChoice>(homeMenuCursor_);
        if (choice==HomeChoice::ScreenOff) asleep_=true;
        if (choice==HomeChoice::Portrait) orientation_=Orientation::Portrait;
        if (choice==HomeChoice::Landscape) orientation_=Orientation::Landscape;
        if (choice==HomeChoice::Development) { mode_=Mode::Development; operationalPage_=0; }
        return choice;
    }
    bool tapOperational(uint32_t heldMs, bool sosActive) {
        if (!operational() || asleep_ || homeMenuOpen_ || sosActive ||
            heldMs>=kShortTapMaxMs) return false;
        operationalPage_=static_cast<uint8_t>((operationalPage_+1U)%kOperationalPageCount);
        return true;
    }
    bool returnFromDevelopment(bool firstPage, uint32_t heldMs, bool sosActive) {
        if (sosActive || operational() || !firstPage || heldMs < kModeToggleMinMs) return false;
        mode_=Mode::Operational;
        operationalPage_=0;
        homeMenuOpen_=false;
        asleep_=false;
        return true;
    }
};
}
