#include "HarmonicGenerator.h"

bool HarmonicGenerator::generateHarmonic(const harmonic_params& params, harmonic_data& data) {
    data.clear();

    quint32 points = static_cast<qint32>(DAC_FREQUENCY / params.frequency);
    if (points < 3) {
        points = 3;
    } else if (points > MAX_POINTS) {
        points = MAX_POINTS;
    }
    double period = 1.0 / params.frequency;
    double dt = period / points;

    data.time.resize(points);
    data.values.resize(points);

    for (int i = 0; i < points; i++) {
        double t = i * dt;
        data.time[i] = t;
        data.values[i] = params.amplitude * sin(2 * M_PI * params.frequency * t + params.phase);
    }

    convertToDacValues(data.values, data.dac_values, params.max_amplitude);

    return true;
}

void HarmonicGenerator::convertToDacValues(const QVector<double>& source, QVector<uint16_t>& dest, double maxAmplitude) {
    if (source.isEmpty()) {
        dest.clear();
        return;
    }

    dest.resize(source.size());
    const double BASE_SCALE = static_cast<double>(DAC_CENTER_VALUE) / maxAmplitude;

    for (int i = 0; i < source.size(); i++) {
        double value = DAC_CENTER_VALUE + (source[i] * BASE_SCALE);
        dest[i] = static_cast<uint16_t>(qBound(0.0, value, static_cast<double>(DAC_MAX_VALUE)));
    }
}

void HarmonicGenerator::expandDataArray(const QVector<uint16_t>& source, QVector<uint16_t>& dest, uint16_t minSize) {
    dest.clear();

    if (source.isEmpty()) {
        return;
    }

    if (source.size() < minSize) {
        uint16_t repeatCount = (minSize + source.size() - 1) / source.size();
        dest.reserve(source.size() * repeatCount);
        for (uint16_t i = 0; i < repeatCount; ++i) {
            dest.append(source);
        }
        qDebug() << "Массив расширен: " << source.size() << "->" << dest.size()
                 << "(повтор x" << repeatCount << ")";
    } else {
        dest = source;
    }
}
