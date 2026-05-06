#pragma once
#include <cmath>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

static constexpr int TABLE_SIZE = 2048;

class WavetableOscillator
{
public:
    float phase          = 0.0f;
    float phaseIncrement = 0.0f;
    std::string waveform = "sine";

    WavetableOscillator() { buildTables(); }

    void setFrequency(float freqHz, float detuneCents = 0.0f, int octave = 0, int semitone = 0)
    {
        float freq = freqHz * std::pow(2.0f, octave + semitone / 12.0f + detuneCents / 1200.0f);
        phaseIncrement = std::max(freq, 0.01f) / 44100.0f; // caller must multiply by (44100/sr)
    }

    // Render n_frames samples into out[], adding to it (caller zeroes first)
    void render(float* out, int nFrames, float freqRatio = 1.0f) const
    {
        const auto& tbl = getTable(waveform);
        float ph  = phase;
        float inc = phaseIncrement * freqRatio;
        for (int i = 0; i < nFrames; ++i)
        {
            float idx  = ph * TABLE_SIZE;
            int   i0   = static_cast<int>(idx) & (TABLE_SIZE - 1);
            int   i1   = (i0 + 1) & (TABLE_SIZE - 1);
            float frac = idx - static_cast<int>(idx);
            out[i] += tbl[i0] * (1.0f - frac) + tbl[i1] * frac;
            ph = std::fmod(ph + inc, 1.0f);
        }
    }

    // Returns new phase after rendering
    float renderAndAdvance(float* out, int nFrames, float freqRatio = 1.0f)
    {
        const auto& tbl = getTable(waveform);
        float ph  = phase;
        float inc = phaseIncrement * freqRatio;
        for (int i = 0; i < nFrames; ++i)
        {
            float idx  = ph * TABLE_SIZE;
            int   i0   = static_cast<int>(idx) & (TABLE_SIZE - 1);
            int   i1   = (i0 + 1) & (TABLE_SIZE - 1);
            float frac = idx - static_cast<int>(idx);
            out[i] += tbl[i0] * (1.0f - frac) + tbl[i1] * frac;
            ph += inc;
            if (ph >= 1.0f) ph -= 1.0f;
        }
        return ph;
    }

private:
    static std::unordered_map<std::string, std::vector<float>> s_tables;
    static bool s_built;

    static const std::vector<float>& getTable(const std::string& name)
    {
        auto it = s_tables.find(name);
        if (it != s_tables.end()) return it->second;
        return s_tables.at("sine");
    }

    static void buildTables()
    {
        if (s_built) return;
        s_built = true;

        constexpr int N = TABLE_SIZE;
        auto makeT = [&](auto fn) {
            std::vector<float> t(N);
            for (int i = 0; i < N; ++i)
                t[i] = fn(i);
            return t;
        };

        const double twoPi = 2.0 * 3.14159265358979323846;

        // Sine
        s_tables["sine"] = makeT([&](int i) {
            return (float)std::sin(twoPi * i / N);
        });

        // Triangle (bandlimited, 12 odd harmonics)
        {
            std::vector<double> tri(N, 0.0);
            for (int n = 1; n <= 23; n += 2)
                for (int i = 0; i < N; ++i)
                    tri[i] += std::pow(-1.0, (n-1)/2.0) * std::sin(n * twoPi * i / N) / (n * n);
            double mx = 0.0;
            for (auto v : tri) mx = std::max(mx, std::abs(v));
            std::vector<float> t(N);
            for (int i = 0; i < N; ++i)
                t[i] = (float)std::max(-1.0, std::min(1.0, tri[i] * (8.0 / (3.14159265358979323846 * 3.14159265358979323846))));
            s_tables["triangle"] = t;
        }

        // Sawtooth (bandlimited, 31 harmonics)
        {
            std::vector<double> saw(N, 0.0);
            for (int n = 1; n <= 31; ++n)
                for (int i = 0; i < N; ++i)
                    saw[i] += std::pow(-1.0, n+1) * std::sin(n * twoPi * i / N) / n;
            std::vector<float> t(N);
            for (int i = 0; i < N; ++i)
                t[i] = (float)std::max(-1.0, std::min(1.0, saw[i] * (2.0 / 3.14159265358979323846)));
            s_tables["sawtooth"] = t;
        }

        // Square (bandlimited, odd harmonics)
        {
            std::vector<double> sq(N, 0.0);
            for (int n = 1; n <= 31; n += 2)
                for (int i = 0; i < N; ++i)
                    sq[i] += std::sin(n * twoPi * i / N) / n;
            std::vector<float> t(N);
            for (int i = 0; i < N; ++i)
                t[i] = (float)std::max(-1.0, std::min(1.0, sq[i] * (4.0 / 3.14159265358979323846)));
            s_tables["square"] = t;
        }

        // Soft: warm pad (sine + soft harmonics)
        {
            std::vector<float> t(N);
            for (int i = 0; i < N; ++i)
            {
                double x = twoPi * i / N;
                t[i] = (float)(std::sin(x)*0.75 + std::sin(2*x)*0.15 + std::sin(3*x)*0.08 + std::sin(4*x)*0.02);
            }
            float mx = 0.0f;
            for (auto v : t) mx = std::max(mx, std::abs(v));
            for (auto& v : t) v /= mx;
            s_tables["soft"] = t;
        }

        // Bell: selective high harmonics, glassy shimmer
        {
            std::vector<float> t(N);
            for (int i = 0; i < N; ++i)
            {
                double x = twoPi * i / N;
                t[i] = (float)(std::sin(x)*0.70 + std::sin(2*x)*0.18 + std::sin(3*x)*0.06
                              + std::sin(5*x)*0.12 + std::sin(8*x)*0.06 + std::sin(10*x)*0.03);
            }
            float mx = 0.0f;
            for (auto v : t) mx = std::max(mx, std::abs(v));
            for (auto& v : t) v /= mx;
            s_tables["bell"] = t;
        }

        // Gamelan: metallic, strong even harmonics + upper partials
        {
            std::vector<float> t(N);
            for (int i = 0; i < N; ++i)
            {
                double x = twoPi * i / N;
                t[i] = (float)(std::sin(x)*0.55 + std::sin(2*x)*0.32 + std::sin(3*x)*0.22
                              + std::sin(5*x)*0.14 + std::sin(7*x)*0.08 + std::sin(11*x)*0.04);
            }
            float mx = 0.0f;
            for (auto v : t) mx = std::max(mx, std::abs(v));
            for (auto& v : t) v /= mx;
            s_tables["gamelan"] = t;
        }

        // Clip all tables to [-1, 1]
        for (auto& [name, tbl] : s_tables)
            for (auto& v : tbl)
                v = std::max(-1.0f, std::min(1.0f, v));
    }
};

inline std::unordered_map<std::string, std::vector<float>> WavetableOscillator::s_tables;
inline bool WavetableOscillator::s_built = false;
