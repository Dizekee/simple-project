#ifndef HARMONIC_GENERATOR_H
#define HARMONIC_GENERATOR_H

#include <cmath>
#include <QDebug>
#include "harmonic.h"

const int DAC_MAX_VALUE = 4096;
const int DAC_CENTER_VALUE = DAC_MAX_VALUE / 2;
const int DAC_FREQUENCY = 3000000;
const int MAX_POINTS = 50000;
const int MIN_DATA_SIZE = 500;

class HarmonicGenerator
{
public:
    HarmonicGenerator() = default;
    bool generateHarmonic(const harmonic_params& params, harmonic_data& data);
    void convertToDacValues(const QVector<double>& source, QVector<uint16_t>& dest, double maxAmplitude);
    void expandDataArray(const QVector<uint16_t>& source, QVector<uint16_t>& dest, uint16_t minSize);
    QString getLastError() const { return m_lastError; }
private:
    QString m_lastError;
};


#endif // HARMONIC_GENERATOR_H
