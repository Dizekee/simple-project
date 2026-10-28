#ifndef DACSETTINGS_H
#define DACSETTINGS_H

#include <QtGlobal>
#include <QVector>

typedef struct {
    double amplitude;
    double frequency;
    double phase;
    double max_amplitude;
} harmonic_params;

typedef struct {
    QVector<double> time;
    QVector<double> values;
    QVector<quint16> dac_values;
    size_t data_size;

    void clear() {
        time.clear();
        values.clear();
        dac_values.clear();
        data_size = 0;
    }
} harmonic_data;

#endif // DACSETTINGS_H
