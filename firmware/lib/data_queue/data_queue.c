/* =====================================================================
 *  data_queue.c — HÀNG ĐỢI VÒNG (C thuần)
 * ===================================================================== */
#include <string.h>
#include "data_queue.h"

void dq_init(data_queue_t *q, void *storage, size_t item_size, uint16_t capacity)
{
    q->buf       = (uint8_t *)storage;
    q->item_size = item_size;
    q->capacity  = capacity;
    q->head      = 0;
    q->count     = 0;
    q->evicted   = 0;
}

void dq_push(data_queue_t *q, const void *item)
{
    uint16_t tail;
    if (q->capacity == 0) return;
    if (q->count == q->capacity) {          /* đầy → bỏ phần tử cũ nhất */
        q->head = (uint16_t)((q->head + 1) % q->capacity);
        q->count--;
        q->evicted++;
    }
    tail = (uint16_t)((q->head + q->count) % q->capacity);
    memcpy(q->buf + (size_t)tail * q->item_size, item, q->item_size);
    q->count++;
}

uint16_t dq_count(const data_queue_t *q)
{
    return q->count;
}

const void *dq_peek(const data_queue_t *q, uint16_t index)
{
    if (index >= q->count) return NULL;
    return q->buf + (size_t)((q->head + index) % q->capacity) * q->item_size;
}

void dq_drop_sent(data_queue_t *q, uint16_t n_sent, uint32_t evicted_at_send)
{
    uint32_t lost = q->evicted - evicted_at_send;   /* đã bị đẩy ra trong lúc gửi */
    uint32_t drop = (n_sent > lost) ? (n_sent - lost) : 0;
    if (drop > q->count) drop = q->count;
    q->head  = (uint16_t)((q->head + drop) % q->capacity);
    q->count = (uint16_t)(q->count - drop);
}
