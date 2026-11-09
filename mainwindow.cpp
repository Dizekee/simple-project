#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_udp_socket(nullptr)
    , m_sampling_frequency(1000000)
    , m_display_points(1000)
    , m_broadcast_channel(0)
    , m_spectrum_size(5000)
{
    ui->setupUi(this);
    defaultSetup();
    setupGraphs();
    setupWaterfall();

    m_udp_socket = new QUdpSocket(this);
    m_validator = new QIntValidator(1, 999999999, this); // validator
    validatorActivation();
    connect(ui->setParams, &QPushButton::clicked, this, &MainWindow::onAddHarmonicClicked);
    connect(ui->switchChannel, &QPushButton::clicked, this, &MainWindow::switchChannel);
    connect(ui->setSpectruSize, &QPushButton::clicked, this, &MainWindow::setSpectrumSize);

    m_buffer.data.resize(CIRCULAR_BUFFER_SIZE);
    m_buffer.data.fill(0);
    m_buffer.full = false;
    m_buffer.index = 0;

    ui->fourierTransform->addGraph();

    m_plot_timer = new QTimer(this);
    connect(m_plot_timer, &QTimer::timeout, this, &MainWindow::updatePlot);
    m_plot_timer->start(100);

    m_fps_timer = new QTimer(this);
    connect(m_fps_timer, &QTimer::timeout, this, &MainWindow::FPSCalculation);
    m_fps_timer->start(1000);

    if (m_udp_socket->bind(QHostAddress::Any, 6666)) {
        connect(m_udp_socket, &QUdpSocket::readyRead, this, &MainWindow::onDataReceived);
        qDebug() << "Сокет привязан к порту 6666";
    } else {
        qDebug() << "Не удалось привязать сокет к порту 6666:" << m_udp_socket->errorString();
    }
}

MainWindow::~MainWindow()
{
    if (m_udp_socket) {
        m_udp_socket->close();
        delete m_udp_socket;
    }
    if (m_plot_timer) {
        m_plot_timer->stop();
        delete m_plot_timer;
    }
    delete ui;
    delete m_validator;
}

void MainWindow::validatorActivation() {    // validator activation
    ui->amplitude->setValidator(m_validator);
    ui->frequency->setValidator(m_validator);
    ui->phase->setValidator(m_validator);
    ui->maxAmplitude->setValidator(m_validator);
    ui->amplitude2->setValidator(m_validator);
    ui->frequency2->setValidator(m_validator);
    ui->phase2->setValidator(m_validator);
    ui->maxAmplitude2->setValidator(m_validator);
    ui->samplingFrequency->setValidator(m_validator);
}

void MainWindow::defaultSetup() {
    ui->amplitude->setText("1.0");         // first dac
    ui->frequency->setText("4000.0");
    ui->phase->setText("0.0");
    ui->maxAmplitude->setText("5.0");

    ui->amplitude2->setText("1.0");       // second dac
    ui->frequency2->setText("4000.0");
    ui->phase2->setText("0.0");
    ui->maxAmplitude2->setText("5.0");

    ui->samplingFrequency->setText("1000000.0"); // adc

    ui->spectrumSize->setText("5000");
}

void MainWindow::setupGraphs() {
    ui->generatedSignal->clearGraphs();
    ui->generatedSignal->xAxis->setLabel("Time, sec");
    ui->generatedSignal->yAxis->setLabel("Amplitude");
    ui->generatedSignal->xAxis->setRange(0, 0.001);
    ui->generatedSignal->yAxis->setRange(-6, 6);
    ui->generatedSignal->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);

    ui->signal->clearGraphs();
    ui->signal->addGraph();
    ui->signal->graph(0)->setPen(QPen(Qt::green, 2));
    ui->signal->xAxis->setLabel("Number of dots");
    ui->signal->yAxis->setLabel("Meaning");
    ui->signal->xAxis->setRange(0, m_display_points);
    ui->signal->yAxis->setRange(0, DAC_MAX_VALUE);
    ui->signal->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
}

void MainWindow::setupWaterfall() {
    waterfallMap = new QCPColorMap(
        ui->waterfall->xAxis,
        ui->waterfall->yAxis);

    QCPColorGradient gradient = QCPColorGradient::gpSpectrum;
    waterfallMap->setGradient(gradient);

    waterfallMap->setDataRange(QCPRange(-100.0, -30.0));

    ui->waterfall->xAxis->setLabel("Частота");
    ui->waterfall->yAxis->setLabel("Время");

    ui->waterfall->xAxis->setRange(0.0, m_spectrum_size);
    ui->waterfall->yAxis->setRange(0.0, HISTORY_DEPTH);
}

void MainWindow::onAddHarmonicClicked() {
    harmonic_params harmonic1, harmonic2;
    uint32_t psc, period;

    if (!readHarmonicParams(0, harmonic1) || !readHarmonicParams(1, harmonic2)) {
        return;
    }

    if (!readSamplingFrequency(m_sampling_frequency)) {
        return;
    }

    m_display_points = static_cast<int>(m_sampling_frequency * DISPLAY_TIME_WINDOW);

    if (m_display_points < 2) {
        m_display_points = 2;
    }

    m_time_step = DISPLAY_TIME_WINDOW / m_display_points;

    harmonic_data data1, data2;
    if (!generator.generateHarmonic(harmonic1, data1) || !generator.generateHarmonic(harmonic2, data2)) {
        QMessageBox::warning(this, "Error", generator.getLastError());
        return;
    }

    if (!calculateTimerParams(static_cast<uint32_t>(m_sampling_frequency), psc, period)) {
        return;
    }
    plotHarmonics(data1, data2);
    sendDataToBoard(data1, data2, psc, period, m_display_points);
    m_buffer.full = false;
}

void MainWindow::sendDataToBoard(const harmonic_data& data1, const harmonic_data& data2, const uint32_t psc, const uint32_t per, int poin) {
    if (!m_udp_socket) {
        m_udp_socket = new QUdpSocket(this);
    }

    QVector<uint16_t> sendData1, sendData2;

    if (!prepareDataForSending(data1, data2, sendData1, sendData2)) {
        return;
    }

    QHostAddress boardAddress("192.168.1.20");
    quint16 boardPort = 6666;

    QByteArray fullData;
    int totalSize = sendData1.size() * sizeof(uint16_t) + sendData2.size() * sizeof(uint16_t);
    fullData.resize(totalSize);

    int offset = 0;
    memcpy(fullData.data() + offset, sendData1.constData(), sendData1.size() * sizeof(uint16_t));
    offset += sendData1.size() * sizeof(uint16_t);
    memcpy(fullData.data() + offset, sendData2.constData(), sendData2.size() * sizeof(uint16_t));

    const int MAX_PACKET_SIZE = 510;
    int totalPackets = (fullData.size() + MAX_PACKET_SIZE - 1) / MAX_PACKET_SIZE;

    for (int packetIndex = 0; packetIndex < totalPackets; ++packetIndex) {
        int startOffset = packetIndex * MAX_PACKET_SIZE;
        int packetSize = qMin(MAX_PACKET_SIZE, fullData.size() - startOffset);

        QByteArray packetData = fullData.mid(startOffset, packetSize);

        QByteArray datagram;
        datagram.resize(7 * sizeof(uint16_t) + packetSize);// 6 7

        int headerOffset = 0;
        uint16_t packetNumber = static_cast<uint16_t>(packetIndex);
        uint16_t totalPacketsCount = static_cast<uint16_t>(totalPackets);
        uint16_t dataSize1 = static_cast<uint16_t>(sendData1.size());
        uint16_t dataSize2 = static_cast<uint16_t>(sendData2.size());
        uint16_t presc = static_cast<uint16_t>(psc);
        uint16_t period = static_cast<uint16_t>(per);
        uint16_t points = static_cast<uint16_t>(poin);

        memcpy(datagram.data() + headerOffset, &packetNumber, sizeof(uint16_t));
        headerOffset += sizeof(uint16_t);
        memcpy(datagram.data() + headerOffset, &totalPacketsCount, sizeof(uint16_t));
        headerOffset += sizeof(uint16_t);
        memcpy(datagram.data() + headerOffset, &dataSize1, sizeof(uint16_t));
        headerOffset += sizeof(uint16_t);
        memcpy(datagram.data() + headerOffset, &dataSize2, sizeof(uint16_t));
        headerOffset += sizeof(uint16_t);
        memcpy(datagram.data() + headerOffset, &presc, sizeof(uint16_t));
        headerOffset += sizeof(uint16_t);
        memcpy(datagram.data() + headerOffset, &period, sizeof(uint16_t));
        headerOffset += sizeof(uint16_t);
        memcpy(datagram.data() + headerOffset, &points, sizeof(uint16_t));
        headerOffset += sizeof(uint16_t);

        memcpy(datagram.data() + headerOffset, packetData.constData(), packetSize);

        qint64 bytesSent = m_udp_socket->writeDatagram(datagram, boardAddress, boardPort);

        if (bytesSent == -1) {
            QMessageBox::warning(this, "Error",
                QString("The package could not be sent %1/%2: %3")
                .arg(packetIndex + 1)
                .arg(totalPackets)
                .arg(m_udp_socket->errorString()));
            return;
        } else {
            qDebug() << "pack" << packetIndex + 1 << "/" << totalPackets
                     << "size:" << datagram.size() << "byte";
        }
    }
    qDebug() << "All packages have been successfully sent!";
}

bool MainWindow::prepareDataForSending(const harmonic_data& data1, const harmonic_data& data2, QVector<uint16_t>& sendData1, QVector<uint16_t>& sendData2) {
    if (data1.values.isEmpty() || data2.values.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Нет данных для отправки");
        return false;

    }

    generator.expandDataArray(data1.dac_values, sendData1, MIN_DATA_SIZE);
    generator.expandDataArray(data2.dac_values, sendData2, MIN_DATA_SIZE);

    if (sendData1.size() > MAX_POINTS || sendData2.size() > MAX_POINTS) {
        QMessageBox::warning(this, "Error",
            QString("There are too many dots!\nMaximum: %1 on the schedule\n"
                    "Schedule 1: %2\nSchedule 2: %3")
            .arg(MAX_POINTS)
            .arg(sendData1.size())
            .arg(sendData2.size()));
        return false;
    }

    return true;
}

void MainWindow::plotHarmonics(const harmonic_data& data1, const harmonic_data& data2) {
    ui->generatedSignal->clearGraphs();
    ui->generatedSignal->addGraph();
    ui->generatedSignal->graph(0)->setData(data1.time, data1.values);
    ui->generatedSignal->graph(0)->setPen(QPen(Qt::blue, 2));

    ui->generatedSignal->addGraph();
    ui->generatedSignal->graph(1)->setData(data2.time, data2.values);
    ui->generatedSignal->graph(1)->setPen(QPen(Qt::red, 2));

    double minFreq = qMin(data1.values.size() > 0 ? 1.0/data1.time.last() : 4000.0, data2.values.size() > 0 ? 1.0/data2.time.last() : 4000.0);
    double maxAmp = qMax(data1.values.size() > 0 ? *std::max_element(data1.values.begin(), data1.values.end()) : 1.0, data2.values.size() > 0 ? *std::max_element(data2.values.begin(), data2.values.end()) : 1.0);

    double period = 1.0 / minFreq;
    ui->generatedSignal->xAxis->setRange(0, period * 1.1);
    ui->generatedSignal->yAxis->setRange(-maxAmp * 1.2, maxAmp * 1.2);
    ui->generatedSignal->replot();
}

bool MainWindow::calculateTimerParams(uint32_t freq, uint32_t &psc, uint32_t &period) {
    if (freq > TIMER_CLOCK || freq < 1) {
        return false;
    }

    psc = 0;
    period = (TIMER_CLOCK / freq) - 1;

    if (period > 65535) {
        psc = (period / 65536) + 1;
        period = static_cast<uint16_t>((TIMER_CLOCK / (freq * (psc + 1))) - 1);
    }
    return true;
}

bool MainWindow::readHarmonicParams(int harmonicIndex, harmonic_params& params) {
    bool ok1, ok2, ok3, ok4;

    QLineEdit* amplitude_edit = (harmonicIndex == 0) ? ui->amplitude : ui->amplitude2;
    QLineEdit* frequency_edit = (harmonicIndex == 0) ? ui->frequency : ui->frequency2;
    QLineEdit* phase_edit = (harmonicIndex == 0) ? ui->phase : ui->phase2;
    QLineEdit* max_amplitude_edit = (harmonicIndex == 0) ? ui->maxAmplitude : ui->maxAmplitude2;

    double amplitude = amplitude_edit->text().toDouble(&ok1);
    double frequency = frequency_edit->text().toDouble(&ok2);
    double phase = phase_edit->text().toDouble(&ok3);
    double max_amplitude = max_amplitude_edit->text().toDouble(&ok4);

    if (!ok1 || !ok2 || !ok3 || !ok4) {
        QMessageBox::warning(this, "Error", QString("Enter the correct numerical values for the harmonica %1") .arg(harmonicIndex + 1));
        return false;
    }

    params.amplitude = amplitude;
    params.frequency = frequency;
    params.phase = phase;
    params.max_amplitude = max_amplitude;

    return true;
}

bool MainWindow::readSamplingFrequency(double &freq) {
    bool ok;
    QLineEdit* samplingFrequencyEdit = ui->samplingFrequency;

    double samplingF = samplingFrequencyEdit->text().toDouble(&ok);

    if (!ok) {
        QMessageBox::warning(this, "Error", QString("Enter the correct sampling rate value"));
        return false;
    }

    freq = samplingF;

    return true;
}

void MainWindow::updatePlot() {
    const int N = static_cast<int>(m_buffer.data.size());

    const int channel = m_broadcast_channel;

    int current_index = m_buffer.index;
    if ((current_index % 2) != channel) {
        current_index = (current_index - 1 + N) % N;
    }

    m_display_times.clear();
    m_display_values.clear();

    m_display_times.resize(m_display_points / 2);
    m_display_values.resize(m_display_points / 2);

    int start = (current_index - 2 * (m_display_points - 1) + N) % N;
    if ((start % 2) != channel) {
        start = (start - 1 + N) % N;
    }

    for (int i = 0; i < m_display_points / 2; ++i) {
        int index = (start + 2 * i) % N;
        m_display_times[i] = i;
        m_display_values[i] = static_cast<double>(m_buffer.data[index]);
    }

    ui->signal->clearGraphs();
    ui->signal->addGraph();
    ui->signal->graph(0)->setData(m_display_times, m_display_values);
    ui->signal->graph(0)->setPen(QPen(Qt::green, 2));

    ui->signal->yAxis->rescale();
    ui->signal->xAxis->setRange(0, m_display_points / 2);

    ui->signal->replot();
}

void MainWindow::onDataReceived() {
    while (m_udp_socket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(m_udp_socket->pendingDatagramSize());

        QHostAddress sender;
        quint16 senderPort;

        qint64 bytesRead = m_udp_socket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);

        if (bytesRead > 0) {
            processReceivedData(datagram);
        }
    }
}

void MainWindow::processReceivedData(const QByteArray& data) {
    static int totalPackets = 0;
    static int totalBytes = 0;
    totalPackets++;
    totalBytes += data.size();

    if (data.size() % sizeof(uint32_t) != 0) {
        qDebug() << "Incorrect data size:" << data.size() << "byte";
        return;
    }

    int numValues = data.size() / sizeof(uint32_t);
    const uint32_t* values = reinterpret_cast<const uint32_t*>(data.constData());

    for (int i = 0; i < numValues; i += 2) {
        addToCircularBuffer( values[i] & 0xFFFF);
        addToCircularBuffer((values[i] >> 16) & 0xFFFF);
        if (m_spectrum_score < m_spectrum_size) { m_spectrum_score++; }
        else {
            plotSpectrum();
            m_spectrum_score = 0;
            m_fps++;
        }
    }
}

void MainWindow::addToCircularBuffer(uint16_t value) {
    m_buffer.data[m_buffer.index] = value;
    if (m_buffer.index + 1 == m_buffer.data.size()) {
        m_buffer.full = true;
    }
    m_buffer.index = (m_buffer.index + 1) % m_buffer.data.size();
}

void MainWindow::switchChannel() {
    m_broadcast_channel = (m_broadcast_channel == 1) ? 2 : 1;
    ui->switchChannel->setText(QString("Channel %1").arg(m_broadcast_channel));
}

void MainWindow::setSpectrumSize() {
    bool ok;
    QLineEdit* spectr_size = ui->spectrumSize;

    quint32 size = spectr_size->text().toUInt(&ok);

    if (!ok) {
        QMessageBox::warning(this, "Error", QString("Enter the correct spectrum size"));
        return;
    }
    m_spectrum_size = size;
    m_spectrum_score = 0;
}

void MainWindow::plotSpectrum() {
    const int buffer_size = static_cast<int>(m_buffer.data.size());
    const int channel = m_broadcast_channel;

    if (m_spectrum_size <= 0 || m_spectrum_size % 2 != 0) {
        qWarning() << "Invalid spectrum size:" << m_spectrum_size;
        return;
    }

    if (m_current_fft_size != m_spectrum_size) {
        initFFT(m_spectrum_size);
    }

    int current_index = m_buffer.index;

    if ((current_index % 2) != channel) {
        current_index = (current_index - 1 + buffer_size) % buffer_size;
    }

    int start = (current_index - 2 * (m_spectrum_size - 1) + buffer_size) % buffer_size;
    if ((start % 2) != channel) {
        start = (start - 1 + buffer_size) % buffer_size;
    }

    for (int i = 0; i < m_spectrum_size; ++i) {
        int index = (start + i * 2) % buffer_size;
        m_fft_in[i] = static_cast<double>(m_buffer.data[index]) / 4096.0;
    }

    fftw_execute(m_fft_plan);

    const int bins = m_spectrum_size / 2 + 1;

    QVector<double> amplitudes(bins);
    for (int i = 0; i < bins; ++i) {
        double re = m_fft_out[i][0];
        double im = m_fft_out[i][1];
        amplitudes[i] = std::sqrt(re * re + im * im) / m_spectrum_size;
    }

    QVector<double> spectrum_dB(bins);
    for (int i = 0; i < bins; ++i) {
        spectrum_dB[i] = (amplitudes[i] > 1e-9) ? 20.0 * std::log10(amplitudes[i]) : -60.0; // -60 дБ вместо -45 для лучшего контраста
    }

    double df = static_cast<double>(m_sampling_frequency) / m_spectrum_size;
    QVector<double> freqs(bins);
    for (int i = 0; i < bins; ++i) {
        freqs[i] = i * df;
    }

    auto *graph = ui->fourierTransform->graph(0);
    if (!graph) {
        ui->fourierTransform->addGraph();
        graph = ui->fourierTransform->graph(0);
        graph->setPen(QPen(Qt::blue, 2));
    }
    graph->setData(freqs, spectrum_dB);

    ui->fourierTransform->xAxis->setLabel("Частота, Гц");
    ui->fourierTransform->yAxis->setLabel("Мощность, дБ");

    ui->fourierTransform->rescaleAxes();
    ui->fourierTransform->replot();

    addHistory(spectrum_dB, df);
}


void MainWindow::addHistory(const QVector<double> &spectrum_dB, double df)
{
    if (spectrum_dB.isEmpty()) return;

    if (newSet) {
        double minValueInSpectrum = *std::min_element(spectrum_dB.begin(), spectrum_dB.end());
        double maxValueInSpectrum = *std::max_element(spectrum_dB.begin(), spectrum_dB.end());
        waterfallMap->setDataRange(QCPRange(minValueInSpectrum, maxValueInSpectrum));
        newSet = false;
    }

    QVector<double> clean = spectrum_dB;
    for (double &v : clean) {
        if (!std::isfinite(v)) v = minValueInSpectrum;
        if (v < minValueInSpectrum) v = minValueInSpectrum;
        if (v > maxValueInSpectrum)  v = maxValueInSpectrum;
    }

    spectrumHistory.append(clean);
    while (spectrumHistory.size() > HISTORY_DEPTH)
        spectrumHistory.removeFirst();

    if (!waterfallMap) return;

    const int rows = spectrumHistory.size();
    const int cols = clean.size();

    if (waterfallMap->data()->keySize() != cols ||
        waterfallMap->data()->valueSize() != rows)
    {
        waterfallMap->data()->setSize(cols, rows);
        waterfallMap->data()->setRange(
            QCPRange(0.0, (cols - 1) * df),
            QCPRange(0.0, rows - 1));
    }

    for (int y = 0; y < rows; y++) {
        const QVector<double>& row = spectrumHistory[y];
        for (int x = 0; x < cols; x++) {
            waterfallMap->data()->setCell(x, rows - 1 - y, row[x]);
        }
    }

    const double xMin = 0.0;
    const double xMax = (cols - 1) * df;
    if (!qFuzzyCompare(ui->waterfall->xAxis->range().lower, xMin) || !qFuzzyCompare(ui->waterfall->xAxis->range().upper, xMax))
    {
        ui->waterfall->xAxis->setRange(xMin, xMax);
    }

    ui->waterfall->replot(QCustomPlot::rpQueuedReplot);
}


void MainWindow::initFFT(int size) {
    if (m_fft_plan) {
        fftw_destroy_plan(m_fft_plan);
        m_fft_plan = nullptr;
    }
    if (m_fft_in) {
        fftw_free(m_fft_in);
        m_fft_in = nullptr;
    }
    if (m_fft_out) {
        fftw_free(m_fft_out);
        m_fft_out = nullptr;
    }

    m_current_fft_size = size;
    m_fft_in  = fftw_alloc_real(size);
    m_fft_out = fftw_alloc_complex(size / 2 + 1);
    m_fft_plan = fftw_plan_dft_r2c_1d(size, m_fft_in, m_fft_out, FFTW_MEASURE);
}

void MainWindow::FPSCalculation() {
    ui->fps->setText(QString("fps: %1").arg(m_fps));
    m_fps = 0;
}
