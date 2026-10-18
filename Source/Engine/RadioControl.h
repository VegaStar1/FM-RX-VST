#pragma once
// RadioControl - «руки» приёмника (по мотивам RTVD/RaTune): SEARCH вверх/вниз, SCAN, LOCK, STOP, REWIND
// (двойной STOP) и политика one-shot. Чистый C++ без JUCE: тестируется офлайн, вызывается из аудиопотока
// раз в блок, до FmBand::process.
//
//  * SEARCH: ручка «едет» по диапазону (с переходом через край), пока не встретит станцию, которую приёмник
//    принимает (отношение несущая/шум >= порога чувствительности), затем мягко доводит на её центр.
//  * SCAN: то же, но на каждой найденной станции стоит scanDwellSec и едет дальше.
//  * LOCK: частота зафиксирована (ручка и поиск игнорируются).
//  * STOP: останавливает поиск/скан; слышимые станции затухают и встают на паузу в начале. Повторная
//    настройка (уйти из зоны станции и вернуться) запускает её с начала.
//  * REWIND (двойной STOP): все станции с начала, настройка на 87.5 МГц (если не LOCK).
//  * One-shot: станция ждёт в начале и стартует, когда на неё настроились; доиграв, выходит из эфира, через
//    паузу начинается поиск от её частоты вверх. Обошли весь диапазон без находки - берём самую сильную из
//    доступных; ту же станцию повторяем, только если других нет. Ушли из зоны доигравшей - она снова ждёт.
//
// Частота во время поиска принадлежит движку (owned): UI/таймер записывает displayFreq() в параметр, а эхо
// этой записи не считается вмешательством. Любое другое движение ручки прерывает поиск.
#include "Engine/FmBand.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace fmrx
{
    enum class RadioCmd : int { None = 0, SearchUp, SearchDown, Scan, Stop, Rewind, Lock, Unlock, Restart };

    struct ControlStation
    {
        bool usable = false;      // включена и есть звук (файл или вход)
        bool oneShot = false;
        bool input = false;       // вход дорожки: всегда играет, one-shot/stop не действуют
        double freqMHz = 98.0;
        float levelDb = -6.f;
    };

    struct ControlParams
    {
        float tuneParam = 98.f;
        float noiseDb = -60.f;
        double cleanZoneHz = 110000.0, noiseZoneHz = 30000.0;
        float sensitivity = 0.5f;   // 0..1: выше - останавливается и на слабых станциях
        float speed = 0.5f;         // 0..1: скорость поиска
        float scanDwellSec = 5.f;
        float endDelaySec = 0.45f;  // пауза после конца 1-SHOT перед автопоиском
    };

    class RadioControl
    {
    public:
        enum class State : int { Idle = 0, Searching = 1, Settling = 2, Scanning = 3, Dwell = 4, Locked = 5 };
        enum class Slot : int { Playing = 0, Parked = 1, Done = 2 };

        static constexpr float kMinMHz = (float) dsp::kBandLowMHz, kMaxMHz = (float) dsp::kBandHighMHz;

        void prepare(double fs) { fs_ = fs; reset(98.f); }
        void reset(float tune)
        {
            state_ = State::Idle; locked_ = false; owned_ = false; started_ = false;
            held_ = target_ = lastParam_ = tune;
            endState_ = 0; endFrom_ = -1; accum_ = 0;
            for (auto& s : slots_) s = {};
            publish();
        }

        // Из любого потока: последняя команда до следующего блока побеждает.
        void post(RadioCmd c) noexcept { pending_.store((int) c, std::memory_order_release); }

        // Аудиопоток, начало блока. Возвращает команду, обработанную в этом блоке (для щелчка кнопки).
        RadioCmd beginBlock(const ControlParams& p, const std::array<ControlStation, kNumStations>& st, int numSamples) noexcept
        {
            p_ = p; st_ = st;
            const double dt = numSamples / fs_;

            // Типы станций могли поменяться (loop <-> one-shot, вход).
            for (int i = 0; i < kNumStations; ++i)
            {
                auto& s = slots_[(size_t) i];
                if (! st[(size_t) i].usable || st[(size_t) i].input) { if (s.state != Slot::Playing) setPlaying(i, false); s.known = false; continue; }
                if (! s.known || s.oneShot != st[(size_t) i].oneShot)
                {
                    s.known = true; s.oneShot = st[(size_t) i].oneShot;
                    if (s.oneShot) park(i, false); else setPlaying(i, true);
                }
            }

            // Ручка: эхо нашей записи - не вмешательство.
            if (! started_) { started_ = true; lastParam_ = held_ = target_ = p.tuneParam; }
            else if (std::fabs(p.tuneParam - lastParam_) > 1e-4f)
            {
                lastParam_ = p.tuneParam;
                if (! locked_)
                {
                    if (! owned_ || std::fabs(p.tuneParam - held_) > 0.02f) { stopMotion(); endState_ = 0; }
                    owned_ = false;
                    target_ = p.tuneParam;
                }
            }

            const auto cmd = (RadioCmd) pending_.exchange(0, std::memory_order_acq_rel);
            switch (cmd)
            {
                case RadioCmd::SearchUp:   if (! locked_) beginMotion(State::Searching, +1); break;
                case RadioCmd::SearchDown: if (! locked_) beginMotion(State::Searching, -1); break;
                case RadioCmd::Scan:       if (! locked_) beginMotion(State::Scanning, +1); break;
                case RadioCmd::Stop:       doStop(); break;
                case RadioCmd::Rewind:     doRewind(); break;
                case RadioCmd::Lock:       stopMotion(); endState_ = 0; locked_ = true; held_ = target_; owned_ = true; break;
                case RadioCmd::Unlock:     locked_ = false; owned_ = false; target_ = p.tuneParam; lastParam_ = p.tuneParam; break;
                case RadioCmd::Restart:    for (int i = 0; i < kNumStations; ++i) restartSlot(i); break;
                case RadioCmd::None:       break;
            }

            // Пауза после конца one-shot -> поиск.
            if (endState_ == 1 && (endDelay_ -= numSamples) <= 0) beginEndSearch();

            // Движение ручки фиксированными шагами 4 мс: результат не зависит от размера блока.
            if (moving())
            {
                accum_ += dt;
                constexpr double kStep = 0.004;
                while (accum_ >= kStep && moving())
                {
                    step((float) kStep);
                    accum_ -= kStep;
                }
                owned_ = true;
            }
            else accum_ = 0;

            if (state_ == State::Dwell)
            {
                dwell_ -= dt;
                if (dwell_ <= 0) beginMotion(State::Scanning, dir_);
            }
            if (owned_) target_ = held_;

            updateSlots();
            publish();
            return cmd;
        }

        // Аудиопоток: плеер станции дошёл до конца файла (one-shot).
        void notifyEnded(int slot) noexcept
        {
            auto& s = slots_[(size_t) slot];
            if (s.state != Slot::Playing || ! s.oneShot) return;
            s.state = Slot::Done;
            s.restartPending = false;
            if (endState_ == 0 && ! locked_ && state_ == State::Idle && tunedGainDb(slot) > -40.f)
            {
                endState_ = 1;
                endFrom_ = slot;
                endDelay_ = (int) (std::clamp(p_.endDelaySec, 0.f, 10.f) * fs_);
            }
        }

        // Аудиопоток: станцию запускают с начала вручную (редактор) - она играет, даже если стояла на паузе.
        void requestPlay(int i) noexcept
        {
            auto& s = slots_[(size_t) i];
            if (! s.known) return;
            s.state = Slot::Playing; s.needLeave = false; s.restartPending = false;
            publish();
        }

        // --- результаты для аудиопотока ---
        float tuneTarget() const noexcept { return target_; }
        Slot slotState(int i) const noexcept { return slots_[(size_t) i].state; }
        // Доигравшая станция продолжает «вещать тишину», пока стрелка не уйдёт с неё: шум нарастает плавно,
        // по мере ухода стрелки через шумовую зону, а не обрывается резким всплеском.
        bool carrierOn(int) const noexcept { return true; }
        // Идёт автоматический переход после 1-SHOT: пауза и поиск (до начала доводки на следующую станцию).
        bool handoverActive() const noexcept { return endState_ == 1 || (endState_ == 2 && state_ == State::Searching); }
        bool consumeRestart(int i) noexcept { auto& s = slots_[(size_t) i]; const bool r = s.restartPending; s.restartPending = false; return r; }

        // --- для UI (любой поток) ---
        float displayFreq() const noexcept { return dispFreq_.load(std::memory_order_relaxed); }
        bool  owned() const noexcept { return ownedA_.load(std::memory_order_relaxed); }
        int   state() const noexcept { return stateA_.load(std::memory_order_relaxed); }
        bool  locked() const noexcept { return lockedA_.load(std::memory_order_relaxed); }
        bool  endSearchActive() const noexcept { return endA_.load(std::memory_order_relaxed); }
        std::uint32_t parkedMask() const noexcept { return parkedA_.load(std::memory_order_relaxed); }
        std::uint32_t doneMask() const noexcept { return doneA_.load(std::memory_order_relaxed); }

        // Чистые функции (для тестов и UI).
        static float cnrDb(const ControlStation& s, float atMHz, const ControlParams& p) noexcept
        {
            if (! s.usable) return -200.f;
            const float z = FmBand::zoneGainDb(((double) atMHz - s.freqMHz) * 1e6, p.cleanZoneHz, p.noiseZoneHz);
            if (z <= FmBand::kZoneOffDb + 0.01f) return -200.f;
            return s.levelDb + z - p.noiseDb;
        }
        static float thresholdDb(float sensitivity) noexcept { return 35.f - 30.f * std::clamp(sensitivity, 0.f, 1.f); }
        static float speedMHzPerSec(float speed) noexcept { const float s = std::clamp(speed, 0.f, 1.f); return 0.6f + 7.4f * s * s; }

    private:
        struct SlotRt
        {
            Slot state = Slot::Playing;
            bool known = false, oneShot = false, needLeave = false, restartPending = false;
        };

        bool moving() const noexcept { return state_ == State::Searching || state_ == State::Scanning || state_ == State::Settling; }

        float tunedGainDb(int i) const noexcept
        {
            return FmBand::zoneGainDb(((double) target_ - st_[(size_t) i].freqMHz) * 1e6, p_.cleanZoneHz, p_.noiseZoneHz);
        }
        bool receivable(int i, float at) const noexcept
        {
            const auto& s = st_[(size_t) i];
            if (! s.usable || slots_[(size_t) i].state == Slot::Done) return false;
            return cnrDb(s, at, p_) >= thresholdDb(p_.sensitivity);
        }
        int bestAt(float at) const noexcept
        {
            int best = -1; float bv = -1e9f;
            for (int i = 0; i < kNumStations; ++i)
                if (receivable(i, at))
                {
                    const float v = cnrDb(st_[(size_t) i], at, p_);
                    if (v > bv) { bv = v; best = i; }
                }
            return best;
        }

        void beginMotion(State s, int dir) noexcept
        {
            state_ = s; dir_ = dir >= 0 ? 1 : -1; found_ = -1; travel_ = 0.f;
            held_ = target_; startFreq_ = target_; owned_ = true;
            armed_ = bestAt(held_) < 0;                    // со станции сначала уходим, потом ищем следующую
        }
        void stopMotion() noexcept { if (state_ != State::Locked) state_ = State::Idle; accum_ = 0; }

        void step(float dt) noexcept
        {
            if (state_ == State::Settling)
            {
                const float to = (float) st_[(size_t) found_].freqMHz;
                const float d = to - held_;
                const float m = 2.0f * dt;                  // доводка 2 МГц/с
                if (std::fabs(d) <= std::max(m, 1e-3f))
                {
                    held_ = to;
                    if (scanMode_) { state_ = State::Dwell; dwell_ = p_.scanDwellSec; }
                    else state_ = State::Idle;
                    if (endState_ == 2) endState_ = 0;
                }
                else held_ += d > 0 ? m : -m;
                return;
            }

            scanMode_ = state_ == State::Scanning;
            if (! armed_ && bestAt(held_) < 0) armed_ = true;
            if (armed_)
            {
                const int b = bestAt(held_);
                if (b >= 0) { found_ = b; state_ = State::Settling; return; }
            }

            const float mv = std::min(speedMHzPerSec(p_.speed) * dt, 0.02f);
            held_ += (float) dir_ * mv;
            travel_ += mv;
            const float span = kMaxMHz - kMinMHz;
            if (held_ > kMaxMHz) held_ -= span;
            if (held_ < kMinMHz) held_ += span;

            if (travel_ > span + 0.5f)                      // полный круг без находки
            {
                if (endState_ == 2) { finishEndSearchFallback(); return; }
                state_ = State::Idle;
                held_ = startFreq_;
            }
        }

        void beginEndSearch() noexcept
        {
            int alternatives = 0;
            for (int i = 0; i < kNumStations; ++i)
                if (i != endFrom_ && st_[(size_t) i].usable && slots_[(size_t) i].state != Slot::Done) ++alternatives;
            if (alternatives > 0)
            {
                endState_ = 2;
                beginMotion(State::Searching, +1);
                armed_ = true;                              // доигравшая станция уже вне эфира
            }
            else
            {
                endState_ = 0;
                park(endFrom_, false);                       // других нет: повтор той же станции
            }
        }

        void finishEndSearchFallback() noexcept
        {
            int best = -1; float bl = -1e9f;
            for (int i = 0; i < kNumStations; ++i)
            {
                if (i == endFrom_ || ! st_[(size_t) i].usable || slots_[(size_t) i].state == Slot::Done) continue;
                if (st_[(size_t) i].levelDb > bl) { bl = st_[(size_t) i].levelDb; best = i; }
            }
            if (best >= 0) held_ = (float) st_[(size_t) best].freqMHz;
            else { held_ = (float) st_[(size_t) endFrom_].freqMHz; park(endFrom_, false); }
            state_ = State::Idle;
            endState_ = 0;
        }

        void setPlaying(int i, bool restart) noexcept
        {
            auto& s = slots_[(size_t) i];
            s.state = Slot::Playing; s.needLeave = false;
            if (restart) s.restartPending = true;
        }
        void park(int i, bool needLeave) noexcept
        {
            auto& s = slots_[(size_t) i];
            s.state = Slot::Parked; s.needLeave = needLeave; s.restartPending = false;
        }
        void restartSlot(int i) noexcept
        {
            auto& s = slots_[(size_t) i];
            if (! s.known) return;
            if (s.oneShot) park(i, false); else setPlaying(i, true);
        }

        void doStop() noexcept
        {
            endState_ = 0;
            stopMotion();
            if (owned_ && ! locked_) target_ = held_;
            for (int i = 0; i < kNumStations; ++i)
            {
                auto& s = slots_[(size_t) i];
                if (! s.known || tunedGainDb(i) < -40.f) continue;   // только слышимые станции
                park(i, true);
            }
        }

        void doRewind() noexcept
        {
            endState_ = 0;
            stopMotion();
            if (! locked_) { held_ = target_ = kMinMHz; owned_ = true; }
            for (int i = 0; i < kNumStations; ++i) restartSlot(i);
        }

        void updateSlots() noexcept
        {
            const bool sweeping = state_ == State::Searching || state_ == State::Scanning;
            for (int i = 0; i < kNumStations; ++i)
            {
                auto& s = slots_[(size_t) i];
                if (! s.known) continue;
                const float g = tunedGainDb(i);
                const bool outOfZone = g <= FmBand::kZoneOffDb + 0.01f;
                if (s.state == Slot::Done)
                {
                    if (outOfZone && ! (endState_ == 1 && endFrom_ == i)) park(i, false);   // ушли - снова ждёт
                }
                else if (s.state == Slot::Parked)
                {
                    if (s.needLeave) { if (outOfZone) s.needLeave = false; }
                    else if (! sweeping && g > -20.f) setPlaying(i, true);   // настроились - с начала
                }
            }
        }

        void publish() noexcept
        {
            std::uint32_t pm = 0, dm = 0;
            for (int i = 0; i < kNumStations; ++i)
            {
                if (slots_[(size_t) i].known && slots_[(size_t) i].state == Slot::Parked) pm |= 1u << i;
                if (slots_[(size_t) i].known && slots_[(size_t) i].state == Slot::Done) dm |= 1u << i;
            }
            dispFreq_.store(target_, std::memory_order_relaxed);
            ownedA_.store(owned_, std::memory_order_relaxed);
            stateA_.store(locked_ ? (int) State::Locked : (int) state_, std::memory_order_relaxed);
            lockedA_.store(locked_, std::memory_order_relaxed);
            endA_.store(endState_ != 0, std::memory_order_relaxed);
            parkedA_.store(pm, std::memory_order_relaxed);
            doneA_.store(dm, std::memory_order_relaxed);
        }

        double fs_ = 48000.0;
        ControlParams p_;
        std::array<ControlStation, kNumStations> st_ {};
        std::array<SlotRt, kNumStations> slots_ {};
        std::atomic<int> pending_ { 0 };

        State state_ = State::Idle;
        bool locked_ = false, owned_ = false, started_ = false, armed_ = true, scanMode_ = false;
        int dir_ = 1, found_ = -1, endState_ = 0, endFrom_ = -1, endDelay_ = 0;
        float held_ = 98.f, target_ = 98.f, lastParam_ = 98.f, startFreq_ = 98.f, travel_ = 0.f;
        double accum_ = 0.0, dwell_ = 0.0;

        std::atomic<float> dispFreq_ { 98.f };
        std::atomic<bool> ownedA_ { false }, lockedA_ { false }, endA_ { false };
        std::atomic<int> stateA_ { 0 };
        std::atomic<std::uint32_t> parkedA_ { 0 }, doneA_ { 0 };
    };
}
