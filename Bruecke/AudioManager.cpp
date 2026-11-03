/**
 * @file AudioManager.cpp
 * @brief Implementierung der Bluetooth-Ausgabe (A2DP-Source + Tonsynthese)
 */

#include "AudioManager.h"

#include <BluetoothA2DPSource.h>
#include <math.h>

namespace {

    BluetoothA2DPSource a2dp;
    AudioManager* owner = nullptr;
    uint32_t beginMs = 0;

    // --- Tonsynthese (nur im BT-Task benutzt) --------------------------------

    constexpr uint32_t REQUEST_MAX_AGE_MS = 1000;

    constexpr uint32_t ms2samples(uint32_t ms) { return ms * Audio::SAMPLE_RATE / 1000; }

    constexpr uint32_t BEEP_SAMPLES    = ms2samples(Audio::BEEP_MS);
    constexpr uint32_t PAUSE_SAMPLES   = ms2samples(Audio::PAUSE_MS);
    constexpr uint32_t PREVIEW_SAMPLES = ms2samples(Audio::PREVIEW_MS);
    constexpr uint32_t RAMP_SAMPLES    = ms2samples(Audio::RAMP_MS);

    // Phaseninkrement für einen 32-Bit-Phasenakku: f / fs · 2^32
    constexpr uint32_t PHASE_STEP =
        (uint32_t)(((uint64_t)Audio::TONE_HZ << 32) / Audio::SAMPLE_RATE);

    int16_t sineTable[256];

    uint32_t phase = 0;
    uint8_t beepsLeft = 0;     // inkl. des gerade klingenden Tons
    bool toneOn = false;
    uint32_t segmentPos = 0;   // Samples seit Segmentbeginn
    uint32_t segmentLen = 0;   // Länge des aktuellen Segments (Ton oder Pause)

    void startTone(uint32_t samples) {
        toneOn = true;
        segmentPos = 0;
        segmentLen = samples;
    }

    // A2DP-Datencallback: liefert frameCount Stereo-Frames. Läuft im BT-Task.
    int32_t onData(Frame* frames, int32_t frameCount) {
        uint8_t count;
        if (owner->takeRequest(count)) {
            if (count > 0) {
                beepsLeft = count;
                startTone(BEEP_SAMPLES);
            } else if (beepsLeft == 0) {
                // Vorhörton: ein einzelner kurzer Ton, nur wenn nichts läuft
                beepsLeft = 1;
                startTone(PREVIEW_SAMPLES);
            }
        }

        const int32_t amp = owner->currentAmplitude();

        for (int32_t i = 0; i < frameCount; i++) {
            int16_t sample = 0;

            if (beepsLeft > 0) {
                if (toneOn) {
                    // Hüllkurve: linear ein- und ausblenden (gegen Knackser)
                    uint32_t edge = segmentPos;
                    uint32_t toEnd = segmentLen - segmentPos;
                    if (toEnd < edge) edge = toEnd;
                    if (edge > RAMP_SAMPLES) edge = RAMP_SAMPLES;

                    int32_t s = (int32_t)sineTable[phase >> 24] * amp / 32767;
                    sample = (int16_t)(s * (int32_t)edge / (int32_t)RAMP_SAMPLES);
                    phase += PHASE_STEP;
                }

                if (++segmentPos >= segmentLen) {
                    if (toneOn) {
                        beepsLeft--;
                        toneOn = false;
                        segmentPos = 0;
                        segmentLen = PAUSE_SAMPLES;
                        if (beepsLeft == 0) {
                            phase = 0;  // fertig, nächste Folge startet im Nulldurchgang
                        }
                    } else {
                        startTone(BEEP_SAMPLES);
                    }
                }
            }

            frames[i].channel1 = sample;
            frames[i].channel2 = sample;
        }
        return frameCount;
    }

    bool onSsid(const char* name, esp_bd_addr_t address, int /*rssi*/) {
        return owner->handleDiscovered(name, address);
    }

} // namespace

void AudioManager::begin(bool scanMode) {
    owner = this;
    scanning = scanMode;
    beginMs = millis();

    for (int i = 0; i < 256; i++) {
        sineTable[i] = (int16_t)lroundf(32767.0f * sinf(2.0f * (float)M_PI * i / 256.0f));
    }

    a2dp.set_local_name(Audio::LOCAL_NAME);
    a2dp.set_data_callback_in_frames(onData);
    a2dp.set_ssid_callback(onSsid);
    // Suchmodus: die zuletzt verbundene Box NICHT wieder ansteuern, sondern
    // Geräte sammeln. Nach der Verbindung speichert die Bibliothek die neue
    // Adresse, der nächste normale Start verbindet dann direkt dorthin.
    a2dp.set_auto_reconnect(!scanMode);
    a2dp.start();

    DEBUG_PRINTF("A2DP-Source gestartet (%s)\n", scanMode ? "Suchmodus" : "Wiederverbinden");
}

AudioManager::State AudioManager::state() const {
    switch (a2dp.get_connection_state()) {
        case ESP_A2D_CONNECTION_STATE_CONNECTED:
            return State::CONNECTED;
        case ESP_A2D_CONNECTION_STATE_CONNECTING:
            return State::CONNECTING;
        default:
            break;
    }
    if (a2dp.is_discovery_active()) {
        return State::SEARCHING;
    }
    // Die Bibliothek wartet nach dem Stack-Start ~10 s, bevor sie sucht oder
    // wiederverbindet; danach versucht ihr Heartbeat alle 10 s neu zu verbinden.
    if (millis() - beginMs < 11000) {
        return State::STARTING;
    }
    return scanning ? State::SEARCHING : State::CONNECTING;
}

void AudioManager::playSignal(uint8_t count) {
    if (count == 0) return;
    portENTER_CRITICAL(&requestLock);
    requestPending = true;
    requestCount = count;
    requestMs = millis();
    portEXIT_CRITICAL(&requestLock);
}

void AudioManager::preview() {
    portENTER_CRITICAL(&requestLock);
    // Ein noch nicht abgeholtes Signal hat Vorrang vor dem Vorhörton
    if (!requestPending) {
        requestPending = true;
        requestCount = 0;
        requestMs = millis();
    }
    portEXIT_CRITICAL(&requestLock);
}

bool AudioManager::takeRequest(uint8_t& count) {
    bool pending;
    portENTER_CRITICAL(&requestLock);
    pending = requestPending;
    count = requestCount;
    uint32_t age = millis() - requestMs;
    requestPending = false;
    portEXIT_CRITICAL(&requestLock);
    // Ohne Verbindung ruft die Bibliothek den Datencallback nicht auf — ein
    // Signal von damals soll nach dem Wiederverbinden nicht nachträglich tönen
    return pending && age <= REQUEST_MAX_AGE_MS;
}

void AudioManager::setVolume(uint8_t step) {
    if (step > Audio::VOLUME_STEPS) step = Audio::VOLUME_STEPS;
    // Quadratische Kennlinie: das Ohr hört logarithmisch, linear wären die
    // oberen Stufen kaum zu unterscheiden
    uint32_t sq = (uint32_t)step * step;
    amplitude = (int16_t)(sq * Audio::AMPLITUDE_MAX
                          / ((uint32_t)Audio::VOLUME_STEPS * Audio::VOLUME_STEPS));
}

//=============================================================================
// Suchmodus
//=============================================================================

bool AudioManager::handleDiscovered(const char* name, const uint8_t* address) {
    if (!scanning) return false;

    if (selectionValid && memcmp(address, selectedAddress, 6) == 0) {
        DEBUG_PRINTF("Gewählte Box gefunden: %s — verbinde\n", name);
        return true;  // die Bibliothek bricht die Discovery ab und verbindet
    }

    if (name == nullptr || name[0] == '\0') return false;

    portENTER_CRITICAL(&deviceLock);
    uint8_t n = devicesFound;
    uint8_t i = 0;
    while (i < n && memcmp(devices[i].address, address, 6) != 0) i++;
    if (i == n && n < Ui::SCAN_MAX_DEVICES) {
        memcpy(devices[i].address, address, 6);
        devicesFound = n + 1;
    }
    if (i < Ui::SCAN_MAX_DEVICES) {
        strlcpy(devices[i].name, name, sizeof(devices[i].name));
    }
    portEXIT_CRITICAL(&deviceLock);

    if (i == n) {
        DEBUG_PRINTF("Gefunden: %s (%02X:%02X:%02X:%02X:%02X:%02X)\n", name,
                     address[0], address[1], address[2], address[3], address[4], address[5]);
    }
    return false;
}

uint8_t AudioManager::deviceCount() const {
    return devicesFound;
}

void AudioManager::deviceName(uint8_t index, char* buf, size_t len) const {
    portENTER_CRITICAL(&deviceLock);
    if (index < devicesFound) {
        strlcpy(buf, devices[index].name, len);
    } else if (len > 0) {
        buf[0] = '\0';
    }
    portEXIT_CRITICAL(&deviceLock);
}

void AudioManager::selectDevice(uint8_t index) {
    portENTER_CRITICAL(&deviceLock);
    if (index < devicesFound) {
        memcpy(selectedAddress, devices[index].address, 6);
        selectionValid = true;
    }
    portEXIT_CRITICAL(&deviceLock);
}
