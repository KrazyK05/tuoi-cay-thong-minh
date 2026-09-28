// API dành cho ESP32. Thiết bị xác thực bằng header "X-Device-Key".
//
// ESP32 gọi POST /api/device/sync vài giây một lần. Một lần gọi làm 3 việc:
//   1) Báo trạng thái hiện tại (nhiệt độ, độ ẩm, đất, bơm, chế độ...)
//   2) Gửi kèm các bản ghi lịch sử và sự kiện bơm đang chờ (kể cả lúc mất mạng)
//   3) Nhận lại cấu hình mới nhất từ web (chế độ, lệnh bơm, ngưỡng)
//
// Đồng bộ cấu hình theo "phiên bản" (config_version):
//   - Web đổi cấu hình  → server tăng version → ESP32 thấy version lớn hơn → áp dụng.
//   - Đổi tại chỗ (nút bấm / web nội bộ ESP32 / ngắt an toàn) → ESP32 giữ nguyên
//     version và gửi cấu hình mới lên → server thấy cùng version nhưng khác nội dung
//     → server chấp nhận cấu hình của thiết bị.
//   - Nếu cả hai cùng đổi trong lúc mất mạng → lệnh trên web được ưu tiên.
const express = require('express');
const rateLimit = require('express-rate-limit');
const { supabase, must, HttpError } = require('../db');
const V = require('../validate');
const { hashDeviceKey, asyncH } = require('../auth');

const router = express.Router();

// Mỗi thiết bị tối đa 120 lần gọi / phút (ESP32 bình thường chỉ ~12 lần).
const deviceLimiter = rateLimit({
  windowMs: 60 * 1000,
  limit: 120,
  keyGenerator: (req) => req.get('x-device-key') || req.ip,
  standardHeaders: 'draft-7',
  legacyHeaders: false,
});

const SETTING_KEYS = ['mode', 'manual_pump', 'soil_low', 'soil_high', 'max_pump_sec', 'cooldown_sec'];
const PUMP_SOURCES = new Set(['auto', 'manual', 'safety', 'local', 'boot', 'cloud']);
const MAX_ITEMS = 60; // số bản ghi tối đa mỗi lần gửi

const requireDevice = asyncH(async (req, res, next) => {
  const key = req.get('x-device-key');
  if (!key || key.length < 20) throw new HttpError(401, 'Thiếu khoá thiết bị');
  const device = must(
    await supabase.from('devices').select('*').eq('api_key_hash', hashDeviceKey(key)).maybeSingle(),
    'device.auth'
  );
  if (!device) throw new HttpError(401, 'Khoá thiết bị không hợp lệ');
  req.device = device;
  next();
});

// Kiểm tra cấu hình thiết bị gửi lên; trả null nếu không hợp lệ.
function parseDeviceCfg(c) {
  if (!c || typeof c !== 'object') return null;
  const n = (v, min, max) => (Number.isInteger(v) && v >= min && v <= max ? v : null);
  const cfg = {
    mode: c.mode === 'auto' || c.mode === 'manual' ? c.mode : null,
    manual_pump: typeof c.manual_pump === 'boolean' ? c.manual_pump : null,
    soil_low: n(c.soil_low, 0, 100),
    soil_high: n(c.soil_high, 0, 100),
    max_pump_sec: n(c.max_pump_sec, 5, 3600),
    cooldown_sec: n(c.cooldown_sec, 0, 86400),
  };
  if (Object.values(cfg).some((v) => v === null) || cfg.soil_low >= cfg.soil_high) return null;
  return { ...cfg, ver: Number.isInteger(c.ver) && c.ver >= 0 ? c.ver : 0 };
}

// Thời điểm ghi nhận: dùng giờ của ESP32 (đã đồng bộ NTP) nếu hợp lý, ngược lại dùng giờ server.
function tsToIso(ts, nowMs) {
  const ms = Number(ts) * 1000;
  if (Number.isFinite(ms) && ms > Date.UTC(2024, 0, 1) && ms <= nowMs + 60000 && ms >= nowMs - 30 * 86400000) {
    return new Date(ms).toISOString();
  }
  return new Date(nowMs).toISOString();
}

router.post('/sync', deviceLimiter, requireDevice, asyncH(async (req, res) => {
  const d = req.device;
  const b = req.body || {};
  const nowMs = Date.now();

  // ---- 1) Hợp nhất cấu hình ----
  const devCfg = parseDeviceCfg(b.cfg);
  const serverCfg = Object.fromEntries(SETTING_KEYS.map((k) => [k, d[k]]));
  const update = {};
  let outCfg = { ...serverCfg, ver: d.config_version };

  if (devCfg) {
    const differs = SETTING_KEYS.some((k) => devCfg[k] !== serverCfg[k]);
    if (devCfg.ver >= d.config_version && differs) {
      // Thiết bị đã có mọi thay đổi của web và vừa đổi tại chỗ → nhận cấu hình của thiết bị.
      const ver = Math.max(devCfg.ver, d.config_version);
      Object.assign(update, Object.fromEntries(SETTING_KEYS.map((k) => [k, devCfg[k]])));
      update.config_version = ver;
      outCfg = { ...Object.fromEntries(SETTING_KEYS.map((k) => [k, devCfg[k]])), ver };
    } else if (devCfg.ver > d.config_version) {
      update.config_version = devCfg.ver; // DB bị tạo lại → theo phiên bản thiết bị
      outCfg.ver = devCfg.ver;
    }
    // Phiên bản ESP32 đang chạy. Nhỏ hơn config_version = lệnh web chưa tới thiết bị.
    update.applied_version = Math.min(devCfg.ver, outCfg.ver);
  }

  // ---- 2) Trạng thái mới nhất ----
  Object.assign(update, {
    temperature: V.num(b.t, -40, 80),
    humidity: V.num(b.h, 0, 100),
    soil_moisture: V.num(b.soil, 0, 100) === null ? null : Math.round(b.soil),
    soil_raw: V.num(b.soil_raw, 0, 4095) === null ? null : Math.round(b.soil_raw),
    pump_state: b.pump === true,
    firmware: typeof b.fw === 'string' ? b.fw.slice(0, 20) : d.firmware,
    local_ip: typeof b.ip === 'string' ? b.ip.slice(0, 45) : d.local_ip,
    rssi: V.num(b.rssi, -120, 0),
    last_seen: new Date(nowMs).toISOString(),
  });
  must(await supabase.from('devices').update(update).eq('id', d.id), 'sync.update');

  // ---- 3) Lịch sử cảm biến & sự kiện bơm ----
  const logs = Array.isArray(b.logs) ? b.logs.slice(0, MAX_ITEMS) : [];
  if (logs.length) {
    must(await supabase.from('sensor_readings').insert(logs.map((l) => ({
      device_id: d.id,
      temperature: V.num(l.t, -40, 80),
      humidity: V.num(l.h, 0, 100),
      soil_moisture: V.num(l.soil, 0, 100) === null ? null : Math.round(l.soil),
      pump_state: l.pump === true,
      created_at: tsToIso(l.ts, nowMs),
    }))), 'sync.logs');
  }

  const events = Array.isArray(b.events) ? b.events.slice(0, MAX_ITEMS) : [];
  if (events.length) {
    must(await supabase.from('pump_events').insert(events.map((e) => ({
      device_id: d.id,
      pump_on: e.on === true,
      source: PUMP_SOURCES.has(e.src) ? e.src : 'unknown',
      created_at: tsToIso(e.ts, nowMs),
    }))), 'sync.events');
  }

  res.json({
    ok: true,
    server_time: Math.floor(nowMs / 1000),
    cfg: outCfg,
    accepted: { logs: logs.length, events: events.length },
  });
}));

module.exports = router;
