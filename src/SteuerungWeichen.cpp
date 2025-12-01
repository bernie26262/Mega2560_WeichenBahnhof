#include "SteuerungWeichen.h"
#include "EventQueue.h"   // NEU: für EVT_FS_COUNTER

SteuerungWeichen::SteuerungWeichen(
    Weiche* weichenArray,
    const SteuerungWeichenDefinition* defs,
    uint8_t numFS
)
: _weichen(weichenArray),
  _defs(defs),
  _numFS(numFS)
{
    if (_numFS > MAX_FS) _numFS = MAX_FS;
    for (uint8_t i = 0; i < _numFS; i++) {
        _counter[i] = 0;
    }
}

void SteuerungWeichen::resetCounter(uint8_t fsIndex) {
    if (fsIndex >= _numFS) return;
    _counter[fsIndex] = 0;
    DBGLN(String("[StW] FS") + fsIndex + " Counter reset");
}

void SteuerungWeichen::onSensorTrigger(uint8_t sensorIndex) {
    // Reset-Sensoren
    for (uint8_t fs = 0; fs < _numFS; fs++) {
        const auto& def = _defs[fs];
        for (uint8_t r = 0; r < def.numResetSensors; r++) {
            if (def.resetSensors[r] == sensorIndex) {
                resetCounter(fs);
                break;
            }
        }
    }

    // Trigger-Fahrstraßen
    for (uint8_t fs = 0; fs < _numFS; fs++) {
        if (_defs[fs].sensorIndex == sensorIndex) {
            handleRouteTrigger(fs);
        }
    }
}

void SteuerungWeichen::handleRouteTrigger(uint8_t fsIndex) {
    if (fsIndex >= _numFS) return;
    const auto& def = _defs[fsIndex];

    uint16_t c = ++_counter[fsIndex];

    DBGLN(String("[StW] FS") + fsIndex +
          " Trigger, Counter=" + String(c));

    // optionales Delta-Event für FS-Counter (8-bit Anzeige reicht meistens)
    pushEvent(EVT_FS_COUNTER, fsIndex, (uint8_t)(c & 0xFF));

    for (uint8_t i = 0; i < def.numSteps; i++) {
        const auto& step = def.steps[i];

        bool shouldTrigger = false;

        if (step.schwelle == 0) {
            shouldTrigger = true;
        } else if (step.schwelle == c) {
            shouldTrigger = true;
        } else if (step.schwelle == 2 && c >= 2) {
            shouldTrigger = true;
        }

        if (!shouldTrigger) continue;

        uint8_t  wid  = step.weicheIndex;
        Richtung ziel = step.ziel;

        DBGLN(String("  [StW] W") + wid +
              " -> " + (ziel == GERADE ? "GERADE" : "ABBIEGEN") +
              " (Schwelle=" + step.schwelle + ")");

        _weichen[wid].ziel = ziel;
        _weichen[wid].schalten();
    }
}

void SteuerungWeichen::getCounters(uint16_t* out, uint8_t max) const {
    uint8_t n = (_numFS < max) ? _numFS : max;
    for (uint8_t i = 0; i < n; i++) {
        out[i] = _counter[i];
    }
    for (uint8_t i = n; i < max; i++) {
        out[i] = 0;
    }
}
