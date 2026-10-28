#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QIntValidator>
#include <QWidget>
#include <QLineEdit>
#include <QDebug>
#include <QUdpSocket>
#include <cstring>
#include "HarmonicGenerator.h"
#include "ringBuffer.h"
#include "harmonic.h"
#include "fftw3.h"
#include "qcustomplot.h"

static constexpr int HISTORY_DEPTH = 100;
static const double DISPLAY_TIME_WINDOW = 0.001;
const int TIMER_CLOCK = 216000000;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;

    static constexpr int CIRCULAR_BUFFER_SIZE = 300000;

    circular_buffer m_buffer;
    QUdpSocket *m_udp_socket;
    HarmonicGenerator generator;
    QIntValidator *m_validator;
    QCPColorMap* waterfallMap = nullptr;
    QVector<QVector<double>> spectrumHistory;
    double m_sampling_frequency;
    int m_display_points;
    QTimer* m_fps_timer;
    double m_time_step;
    QTimer* m_plot_timer;
    QVector<double> m_display_times;
    QVector<double> m_display_values;
    uint8_t m_broadcast_channel;
    quint32 m_spectrum_size;
    quint32 m_spectrum_score;
    quint32 m_fps;
    fftw_plan m_fft_plan = nullptr;
    double* m_fft_in = nullptr;
    fftw_complex* m_fft_out = nullptr;
    int m_current_fft_size = 0;
    bool newSet = false;
    double maxValueInSpectrum = -30;
    double minValueInSpectrum = -100;


    void setTheHarmonicsParams();
    void validatorActivation();
    bool validateAllParams();
    bool readUint32(QLineEdit *edit, quint32 &item);
    void defaultSetup();
    void setupGraphs();
    void onAddHarmonicClicked();
    bool readHarmonicParams(int harmonicIndex, harmonic_params& params);
    bool readSamplingFrequency(double &freq);
    bool calculateTimerParams(uint32_t freq, uint32_t &psc, uint32_t &period);
    void plotHarmonics(const harmonic_data& data1, const harmonic_data& data2);
    void sendDataToBoard(const harmonic_data& data1, const harmonic_data& data2, const uint32_t psc, const uint32_t per, int poin);
    bool prepareDataForSending(const harmonic_data& data1, const harmonic_data& data2, QVector<uint16_t>& sendData1, QVector<uint16_t>& sendData2);
    void updatePlot();
    void onDataReceived();
    void processReceivedData(const QByteArray& data);
    void addToCircularBuffer(uint16_t value);
    void switchChannel();
    void plotSpectrum();
    void setSpectrumSize();
    void addHistory(const QVector<double> &spectrum_dB, double df);
    void FPSCalculation();
    void setupWaterfall();
    void initFFT(int size);

};
#endif // MAINWINDOW_H
