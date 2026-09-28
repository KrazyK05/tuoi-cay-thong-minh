/* =====================================================================
 *  UNIT TEST cho thuật toán tưới và hàng đợi — chạy trên máy tính:
 *      pio test -e native
 * ===================================================================== */
#include <unity.h>
#include "irrigation_logic.h"
#include "data_queue.h"
#include "time_utils.h"

static settings_t s;
static readings_t r;
static irr_input_t in;

void setUp(void)
{
    irr_default_settings(&s, 35, 60, 60, 300);
    r.temperature = 28.0f; r.humidity = 70.0f;
    r.soil_percent = 50; r.soil_raw = 2200; r.soil_ok = true;
    in.settings = &s; in.readings = &r;
    in.pump_on = false; in.run_ms = 0; in.ever_run = false;
    in.since_stop_ms = 0; in.manual_source = SRC_MANUAL;
}
void tearDown(void) {}

/* ---------------- AUTO ---------------- */
static void test_auto_turns_on_when_dry(void)
{
    r.soil_percent = 30;
    irr_decision_t d = irr_decide(&in);
    TEST_ASSERT_EQUAL(IRR_TURN_ON, d.action);
    TEST_ASSERT_EQUAL(SRC_AUTO, d.source);
}

static void test_auto_hysteresis_keeps_running_between_thresholds(void)
{
    in.pump_on = true; in.run_ms = 10000; r.soil_percent = 45;   /* 35 <= 45 < 60 */
    TEST_ASSERT_EQUAL(IRR_KEEP, irr_decide(&in).action);
    in.pump_on = false; in.ever_run = true; in.since_stop_ms = 999999;
    TEST_ASSERT_EQUAL(IRR_KEEP, irr_decide(&in).action);          /* tắt rồi thì không tự bật lại ở 45% */
}

static void test_auto_turns_off_when_wet(void)
{
    in.pump_on = true; in.run_ms = 10000; r.soil_percent = 60;
    irr_decision_t d = irr_decide(&in);
    TEST_ASSERT_EQUAL(IRR_TURN_OFF, d.action);
    TEST_ASSERT_EQUAL(SRC_AUTO, d.source);
}

static void test_auto_respects_cooldown(void)
{
    r.soil_percent = 20; in.ever_run = true; in.since_stop_ms = 100000;   /* < 300 s */
    TEST_ASSERT_EQUAL(IRR_KEEP, irr_decide(&in).action);
    TEST_ASSERT_EQUAL_UINT32(200, irr_cooldown_left_sec(&s, true, false, 100000));
    in.since_stop_ms = 300000;
    TEST_ASSERT_EQUAL(IRR_TURN_ON, irr_decide(&in).action);
    TEST_ASSERT_EQUAL_UINT32(0, irr_cooldown_left_sec(&s, true, false, 300000));
}

static void test_auto_sensor_fault_never_turns_on_and_stops_pump(void)
{
    r.soil_percent = 0; r.soil_ok = false;
    TEST_ASSERT_EQUAL(IRR_KEEP, irr_decide(&in).action);
    in.pump_on = true; in.run_ms = 1000;
    TEST_ASSERT_EQUAL(IRR_TURN_OFF, irr_decide(&in).action);
}

/* ---------------- An toàn ---------------- */
static void test_safety_cutoff_auto(void)
{
    in.pump_on = true; in.run_ms = 60000; r.soil_percent = 10;
    irr_decision_t d = irr_decide(&in);
    TEST_ASSERT_EQUAL(IRR_TURN_OFF, d.action);
    TEST_ASSERT_EQUAL(SRC_SAFETY, d.source);
    TEST_ASSERT_FALSE(d.clear_manual);
}

static void test_safety_cutoff_manual_clears_command(void)
{
    s.auto_mode = false; s.manual_pump = true;
    in.pump_on = true; in.run_ms = 60000;
    irr_decision_t d = irr_decide(&in);
    TEST_ASSERT_EQUAL(IRR_TURN_OFF, d.action);
    TEST_ASSERT_EQUAL(SRC_SAFETY, d.source);
    TEST_ASSERT_TRUE(d.clear_manual);
}

/* ---------------- MANUAL ---------------- */
static void test_manual_follows_command_with_source(void)
{
    s.auto_mode = false; s.manual_pump = true; r.soil_percent = 90;   /* đất ướt vẫn bơm theo lệnh */
    in.manual_source = SRC_LOCAL;
    irr_decision_t d = irr_decide(&in);
    TEST_ASSERT_EQUAL(IRR_TURN_ON, d.action);
    TEST_ASSERT_EQUAL(SRC_LOCAL, d.source);
    in.pump_on = true; in.run_ms = 1000;
    TEST_ASSERT_EQUAL(IRR_KEEP, irr_decide(&in).action);
    s.manual_pump = false;
    TEST_ASSERT_EQUAL(IRR_TURN_OFF, irr_decide(&in).action);
}

/* ---------------- Tiện ích ---------------- */
static void test_soil_percent_mapping(void)
{
    TEST_ASSERT_EQUAL_INT(0,   irr_soil_percent(3200, 3200, 1300));
    TEST_ASSERT_EQUAL_INT(100, irr_soil_percent(1300, 3200, 1300));
    TEST_ASSERT_EQUAL_INT(50,  irr_soil_percent(2250, 3200, 1300));
    TEST_ASSERT_EQUAL_INT(0,   irr_soil_percent(4000, 3200, 1300));   /* khô hơn mốc → kẹp 0 */
    TEST_ASSERT_EQUAL_INT(100, irr_soil_percent(900, 3200, 1300));    /* ướt hơn mốc → kẹp 100 */
    TEST_ASSERT_FALSE(irr_soil_raw_valid(0));
    TEST_ASSERT_FALSE(irr_soil_raw_valid(4095));
    TEST_ASSERT_TRUE(irr_soil_raw_valid(2000));
}

static void test_settings_validation(void)
{
    TEST_ASSERT_TRUE(irr_settings_valid(35, 60, 60, 300));
    TEST_ASSERT_FALSE(irr_settings_valid(60, 60, 60, 300));   /* bật phải < tắt */
    TEST_ASSERT_FALSE(irr_settings_valid(35, 101, 60, 300));
    TEST_ASSERT_FALSE(irr_settings_valid(35, 60, 4, 300));
    TEST_ASSERT_FALSE(irr_settings_valid(35, 60, 60, 90000));
    TEST_ASSERT_EQUAL_STRING("safety", irr_source_name(SRC_SAFETY));
}

/* ---------------- Hàng đợi ---------------- */
static void test_queue_push_peek_and_overflow(void)
{
    int storage[3];
    data_queue_t q;
    int i;
    dq_init(&q, storage, sizeof(int), 3);
    for (i = 1; i <= 5; i++) dq_push(&q, &i);          /* 1 2 3 4 5 → giữ 3 4 5 */
    TEST_ASSERT_EQUAL_UINT16(3, dq_count(&q));
    TEST_ASSERT_EQUAL_INT(3, *(const int *)dq_peek(&q, 0));
    TEST_ASSERT_EQUAL_INT(5, *(const int *)dq_peek(&q, 2));
    TEST_ASSERT_NULL(dq_peek(&q, 3));
    TEST_ASSERT_EQUAL_UINT32(2, q.evicted);
}

static void test_queue_drop_sent_handles_overflow_during_send(void)
{
    int storage[4];
    data_queue_t q;
    int i;
    uint32_t snap;
    dq_init(&q, storage, sizeof(int), 4);
    for (i = 1; i <= 4; i++) dq_push(&q, &i);           /* 1 2 3 4 (đầy) */
    snap = q.evicted;                                   /* chụp để gửi 1,2 */
    i = 5; dq_push(&q, &i);                             /* trong lúc gửi: 1 bị đẩy ra → 2 3 4 5 */
    dq_drop_sent(&q, 2, snap);                          /* chỉ còn phải xoá 2 */
    TEST_ASSERT_EQUAL_UINT16(3, dq_count(&q));
    TEST_ASSERT_EQUAL_INT(3, *(const int *)dq_peek(&q, 0));

    snap = q.evicted;
    dq_drop_sent(&q, 10, snap);                         /* xoá quá số lượng → rỗng, không lỗi */
    TEST_ASSERT_EQUAL_UINT16(0, dq_count(&q));
}

/* ---------------- Ngày giờ (dùng cho DS1302) ---------------- */
static void check_epoch(uint16_t y, uint8_t mo, uint8_t d, uint8_t h, uint8_t mi, uint8_t s_, uint32_t epoch, uint8_t wd)
{
    datetime_t dt = { y, mo, d, h, mi, s_, 0 }, back;
    TEST_ASSERT_TRUE(tu_datetime_valid(&dt));
    TEST_ASSERT_EQUAL_UINT32(epoch, tu_datetime_to_epoch(&dt));
    tu_epoch_to_datetime(epoch, &back);
    TEST_ASSERT_EQUAL_UINT16(y, back.year);
    TEST_ASSERT_EQUAL_UINT8(mo, back.month);
    TEST_ASSERT_EQUAL_UINT8(d, back.day);
    TEST_ASSERT_EQUAL_UINT8(h, back.hour);
    TEST_ASSERT_EQUAL_UINT8(mi, back.minute);
    TEST_ASSERT_EQUAL_UINT8(s_, back.second);
    TEST_ASSERT_EQUAL_UINT8(wd, back.weekday);
}

static void test_epoch_conversion_known_dates(void)
{
    check_epoch(2000, 1, 1, 0, 0, 0, 946684800UL, 6);      /* Thứ Bảy */
    check_epoch(2024, 2, 29, 12, 30, 45, 1709209845UL, 4); /* năm nhuận, Thứ Năm */
    check_epoch(2026, 9, 28, 18, 18, 0, 1790619480UL, 1);  /* Thứ Hai */
    check_epoch(2099, 12, 31, 23, 59, 59, 4102444799UL, 4);
}

static void test_epoch_roundtrip_every_day(void)
{
    uint32_t e;
    datetime_t dt;
    for (e = 946684800UL; e < 4102444800UL; e += 86400UL + 3661UL) {  /* nhảy >1 ngày, lệch giờ */
        tu_epoch_to_datetime(e, &dt);
        TEST_ASSERT_TRUE(tu_datetime_valid(&dt));
        TEST_ASSERT_EQUAL_UINT32(e, tu_datetime_to_epoch(&dt));
    }
}

static void test_bcd_and_validation(void)
{
    datetime_t bad = { 2025, 2, 29, 0, 0, 0, 0 };            /* 2025 không nhuận */
    TEST_ASSERT_EQUAL_UINT8(59, tu_bcd_to_dec(0x59));
    TEST_ASSERT_EQUAL_UINT8(0x47, tu_dec_to_bcd(47));
    TEST_ASSERT_FALSE(tu_datetime_valid(&bad));
    bad.day = 28;
    TEST_ASSERT_TRUE(tu_datetime_valid(&bad));
    bad.hour = 24;
    TEST_ASSERT_FALSE(tu_datetime_valid(&bad));
    bad.hour = 0; bad.year = 2165;                           /* giá trị rác khi chưa nối dây (0xFF) */
    TEST_ASSERT_FALSE(tu_datetime_valid(&bad));
}

static void test_parse_datetime_for_settime_command(void)
{
    datetime_t dt;
    TEST_ASSERT_TRUE(tu_parse_datetime("2026-09-29 01:45:00", &dt));
    TEST_ASSERT_EQUAL_UINT16(2026, dt.year);
    TEST_ASSERT_EQUAL_UINT8(9, dt.month);
    TEST_ASSERT_EQUAL_UINT8(29, dt.day);
    TEST_ASSERT_EQUAL_UINT8(1, dt.hour);
    TEST_ASSERT_EQUAL_UINT8(45, dt.minute);
    TEST_ASSERT_EQUAL_UINT8(2, dt.weekday);                      /* 29/9/2026 là Thứ Ba */
    TEST_ASSERT_TRUE(tu_parse_datetime(" 2024/02/29T23:59:59\r\n", &dt));
    TEST_ASSERT_FALSE(tu_parse_datetime("2025-02-29 10:00:00", &dt));   /* ngày không tồn tại */
    TEST_ASSERT_FALSE(tu_parse_datetime("2026-9-29 01:45:00", &dt));    /* thiếu số 0 */
    TEST_ASSERT_FALSE(tu_parse_datetime("2026-09-29 01:45", &dt));
    TEST_ASSERT_FALSE(tu_parse_datetime("2026-09-29 01:45:00 abc", &dt));
    TEST_ASSERT_EQUAL_UINT32(7, tu_abs_diff(3, 10));
    TEST_ASSERT_EQUAL_UINT32(7, tu_abs_diff(10, 3));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_auto_turns_on_when_dry);
    RUN_TEST(test_auto_hysteresis_keeps_running_between_thresholds);
    RUN_TEST(test_auto_turns_off_when_wet);
    RUN_TEST(test_auto_respects_cooldown);
    RUN_TEST(test_auto_sensor_fault_never_turns_on_and_stops_pump);
    RUN_TEST(test_safety_cutoff_auto);
    RUN_TEST(test_safety_cutoff_manual_clears_command);
    RUN_TEST(test_manual_follows_command_with_source);
    RUN_TEST(test_soil_percent_mapping);
    RUN_TEST(test_settings_validation);
    RUN_TEST(test_queue_push_peek_and_overflow);
    RUN_TEST(test_queue_drop_sent_handles_overflow_during_send);
    RUN_TEST(test_epoch_conversion_known_dates);
    RUN_TEST(test_epoch_roundtrip_every_day);
    RUN_TEST(test_bcd_and_validation);
    RUN_TEST(test_parse_datetime_for_settime_command);
    return UNITY_END();
}
