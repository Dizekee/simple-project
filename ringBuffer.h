#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <QtGlobal>

typedef struct {
    QVector<uint16_t> data;
    quint32 buffer_size;
    quint32 head;
    quint32 tail;
    quint32 index;
    bool full;
} circular_buffer;

#endif // RING_BUFFER_H
