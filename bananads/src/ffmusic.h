// kms.ff_music: the sound clock while fast-forwarding (melonDS patch 0500, Emu::set_sound_clock). The game runs N
// times as fast; the ARM7's timers and the SPU run at 1/N of its time, so the music and the sound effects keep
// their tempo and pitch and the SPU makes about real time's worth of sound. N is what the game reaches (measured
// from the frames' times against the pacer's period; Max, or a game the handheld can't run at the speed asked, is
// slower than asked), nudged by the sound thread's backlog against its target: more queued than the target, a
// little less sound per frame; less, a little more. Only the amount per frame changes: the sequencer and the SPU
// are on the same clock, so what plays keeps its tempo and pitch whatever the nudge.
#pragma once
#include <algorithm>
#include <cmath>

namespace bananads {

class FfMusic {
public:
    static constexpr double kMaxRatio = 64.0;  // the sound clock's lowest rate is 1/64 (NDS::SetSoundClockRate)

    double rate() const { return sent_; }  // the sound clock to set: 1 / the ratio

    // fast-forward on at `speed` (2, 3, or 99 for Max)
    void start(int speed) {
        nominal_ = speed >= 99 ? kMaxRatio : std::max(1, speed);
        measured_ = std::min(nominal_, 3.0);  // (Max: a first guess; the frames' times take over within ~0.1 s)
        last_us_ = 0;
        sent_ = 0;
    }

    // each frame while fast-forwarding, before it runs: now, the pacer's period at normal speed (us), the sound
    // queued and its target (stereo frames; target 0: no servo, no nudge). True when rate() moved enough to set
    bool frame(long long now_us, double period_us, int backlog, int target) {
        if (last_us_) {
            const long long dt = now_us - last_us_;
            // (a pause, a menu or a state's save or load: not the game's speed)
            if (dt > 0 && dt < 500000) measured_ += (period_us / (double)dt - measured_) * 0.1;
        }
        last_us_ = now_us;
        double ratio = std::min(nominal_, measured_);
        if (target > 0) {
            const double err = std::clamp((double)(backlog - target) / target, -1.0, 1.0);
            ratio *= 1.0 + 0.25 * err;
        }
        ratio = std::clamp(ratio, 1.0, kMaxRatio);
        const double want = 1.0 / ratio;
        // (changes below 0.4% wait: each one is a full Mix for FastMix, and the SPU's count per Mix moves in
        // steps of 1/512 anyway)
        if (sent_ > 0 && std::fabs(want - sent_) < 0.004 * sent_) return false;
        sent_ = want;
        return true;
    }

private:
    double nominal_ = 2.0, measured_ = 2.0, sent_ = 0;
    long long last_us_ = 0;
};

}  // namespace bananads
