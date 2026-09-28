/* =====================================================================
 *  button — NÚT NHẤN TẠI CHỖ (chống dội phím, phân biệt nhấn ngắn / giữ)
 * ===================================================================== */
#ifndef BUTTON_H
#define BUTTON_H

typedef enum {
    BTN_NONE = 0,
    BTN_SHORT,     /* nhấn rồi nhả trước 2 giây */
    BTN_LONG       /* giữ đủ 2 giây (báo 1 lần, ngay khi đủ thời gian) */
} button_event_t;

void button_init(void);

/* Gọi liên tục trong loop(); trả về sự kiện nếu có */
button_event_t button_poll(void);

#endif /* BUTTON_H */
