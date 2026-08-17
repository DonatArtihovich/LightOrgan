#pragma once
#include <SFML/Audio.hpp>
#include <vector>
#include <cmath>
#include <atomic>
#include <random>

#define ACTIVE_VOICES (7)

class AscensionStream : public sf::SoundStream {
public:
    struct VoiceData {
        float frequency = 0.0f;
        float volume = 0.0f;
    };

private:
    struct Voice {
        float targetFreq = 0.0f;
        float currentFreq = 0.0f;
        float targetVolume = 0.0f;
        float currentVolume = 0.0f;
        float envelope = 0.0f;
        double phase = 0.0;
        double phaseDetune = 0.0;
    };

public:
    AscensionStream() : m_masterVolume(0.0f) {
        m_voices.resize(ACTIVE_VOICES);
        
        initialize(2, 44100, { sf::SoundChannel::FrontLeft, sf::SoundChannel::FrontRight });

        // Asymmetric ambient delay
        m_delayBufferLeft.resize(18500, 0.0f);
        m_delayBufferRight.resize(27000, 0.0f);
        m_writePtrLeft = 0;
        m_writePtrRight = 0;
    }

    void setPolyphony(const std::vector<VoiceData>& dataPack) {
        for (size_t i = 0; i < ACTIVE_VOICES; ++i) {
            if (i < dataPack.size()) {
                if (dataPack[i].frequency > 20.0f && dataPack[i].volume > 0.001f) {
                    if (m_voices[i].targetVolume <= 0.001f) {
                        m_voices[i].envelope = 0.01f; // Envelope reset
                        m_voices[i].currentFreq = dataPack[i].frequency * 0.85f;
                    }
                    m_voices[i].targetFreq = dataPack[i].frequency;
                    m_voices[i].targetVolume = dataPack[i].volume;
                } else {
                    m_voices[i].targetVolume = 0.0f; // Voice releasing
                }
            }
        }
    }

    void setVolumeLevel(float volume) {
        if (volume < 0.0f) volume = 0.0f;
        if (volume > 1.0f) volume = 1.0f;
        m_masterVolume = volume;
    }

protected:
    bool onGetData(Chunk& data) override {
        const size_t numFrames = 1100; 
        m_samples.resize(numFrames * 2);

        const double sampleRate = 44100.0;
        const double pi = 3.14159265358979323846;

        static std::mt19937 gen(1337);
        static std::uniform_real_distribution<float> dis(-1.0f, 1.0f);

        for (size_t i = 0; i < numFrames; ++i) {
            double leftAccumulator = 0.0;
            double rightAccumulator = 0.0;
            int activeVoicesCount = 0;

            static double internalTime = 0.0;
            internalTime += 1.0 / sampleRate;

            double lfo = std::sin(2.0 * pi * 0.25 * internalTime);

            for (size_t v = 0; v < ACTIVE_VOICES; ++v) {
                Voice& voice = m_voices[v];

                // Volume level aligning
                voice.currentVolume += (voice.targetVolume - voice.currentVolume) * 0.005f;

                // Envelope
                if (voice.currentVolume > 0.001f) {
                    if (voice.envelope < 1.0f) voice.envelope += 0.00002f; 
                } else {
                    if (voice.envelope > 0.0f) voice.envelope -= 0.00001f;
                }

                if (voice.envelope <= 0.0f && voice.currentVolume <= 0.001f) continue;

                activeVoicesCount++;

                // Frequency aligning
                voice.currentFreq += (voice.targetFreq - voice.currentFreq) * 0.001f;

                // Pitch Glide) for each tone
                double pitchGlide = 0.90f + (0.50f * std::pow(voice.envelope, 1.5));
                double baseFreq = voice.currentFreq * pitchGlide;

                // Chorus
                double freqLeft = baseFreq * (0.998 + lfo * 0.001);
                double freqRight = baseFreq * (1.002 - lfo * 0.001);

                double waveL = std::sin(voice.phase);
                double waveR = std::sin(voice.phaseDetune);
                double waveCrystal = std::sin(voice.phase * 4.0); // glass air

                float combinedVolume = voice.currentVolume * std::pow(voice.envelope, 2.0f);
                
                leftAccumulator  += ((waveL * 0.6) + (waveCrystal * 0.15)) * combinedVolume;
                rightAccumulator += ((waveR * 0.6) + (waveCrystal * 0.15)) * combinedVolume;

                voice.phase += 2.0 * pi * freqLeft / sampleRate;
                voice.phaseDetune += 2.0 * pi * freqRight / sampleRate;
                
                if (voice.phase > 2.0 * pi) voice.phase -= 2.0 * pi;
                if (voice.phaseDetune > 2.0 * pi) voice.phaseDetune -= 2.0 * pi;
            }

            if (activeVoicesCount > 1) {
                leftAccumulator /= std::sqrt(activeVoicesCount);
                rightAccumulator /= std::sqrt(activeVoicesCount);
            }

            double leftSignal = leftAccumulator * m_masterVolume;
            double rightSignal = rightAccumulator * m_masterVolume;

            // Stereo delay
            float delayedLeft = m_delayBufferLeft[m_writePtrLeft];
            float delayedRight = m_delayBufferRight[m_writePtrRight];

            float finalLeft = leftSignal + delayedLeft * 0.50f;
            float finalRight = rightSignal + delayedRight * 0.50f;

            m_delayBufferLeft[m_writePtrLeft] = leftSignal + delayedRight * 0.65f;
            m_delayBufferRight[m_writePtrRight] = rightSignal + delayedLeft * 0.65f;

            m_writePtrLeft = (m_writePtrLeft + 1) % m_delayBufferLeft.size();
            m_writePtrRight = (m_writePtrRight + 1) % m_delayBufferRight.size();

            if (finalLeft > 1.0f) finalLeft = 1.0f; if (finalLeft < -1.0f) finalLeft = -1.0f;
            if (finalRight > 1.0f) finalRight = 1.0f; if (finalRight < -1.0f) finalRight = -1.0f;

            m_samples[i * 2] = static_cast<int16_t>(finalLeft * 32767.0f);
            m_samples[i * 2 + 1] = static_cast<int16_t>(finalRight * 32767.0f);
        }

        data.samples = m_samples.data();
        data.sampleCount = m_samples.size();
        return true;
    }

    void onSeek(sf::Time timeOffset) override {}

private:
    std::vector<int16_t> m_samples;
    std::atomic<float> m_masterVolume;
    std::vector<Voice> m_voices;

    std::vector<float> m_delayBufferLeft;
    std::vector<float> m_delayBufferRight;
    size_t m_writePtrLeft;
    size_t m_writePtrRight;
};
