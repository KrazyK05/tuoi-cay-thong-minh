/* =====================================================================
 *  data_queue — HÀNG ĐỢI VÒNG (ring buffer) dùng chung, C thuần
 *
 *  Giữ dữ liệu khi mất mạng; đầy thì tự bỏ phần tử CŨ NHẤT.
 *  Dùng cho cả bản ghi lịch sử (log_item_t) và sự kiện bơm (event_item_t).
 *  Không tự khoá — người gọi phải giữ khoá khi dùng từ 2 nhân.
 * ===================================================================== */
#ifndef DATA_QUEUE_H
#define DATA_QUEUE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t  *buf;          /* vùng nhớ do người dùng cấp       */
    size_t    item_size;    /* kích thước 1 phần tử (byte)      */
    uint16_t  capacity;     /* số phần tử tối đa                */
    uint16_t  head;         /* vị trí phần tử cũ nhất           */
    uint16_t  count;        /* số phần tử hiện có               */
    uint32_t  evicted;      /* tổng số phần tử đã bị đẩy ra do đầy */
} data_queue_t;

void        dq_init(data_queue_t *q, void *storage, size_t item_size, uint16_t capacity);
void        dq_push(data_queue_t *q, const void *item);   /* đầy → bỏ phần tử cũ nhất */
uint16_t    dq_count(const data_queue_t *q);
const void *dq_peek(const data_queue_t *q, uint16_t index); /* 0 = cũ nhất; NULL nếu ngoài phạm vi */

/* Xoá n phần tử đầu sau khi đã gửi thành công.
 * evicted_at_send = giá trị q->evicted lúc chụp dữ liệu để gửi: nếu trong lúc
 * gửi hàng đợi bị tràn (phần tử cũ đã bị đẩy ra) thì chỉ xoá phần còn lại,
 * không xoá nhầm dữ liệu mới chưa gửi. */
void        dq_drop_sent(data_queue_t *q, uint16_t n_sent, uint32_t evicted_at_send);

#ifdef __cplusplus
}
#endif

#endif /* DATA_QUEUE_H */
