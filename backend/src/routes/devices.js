// Quản lý thiết bị & điều khiển từ web (dành cho người dùng đã đăng nhập).
// Người dùng thường chỉ thấy thiết bị của mình; admin thấy tất cả.
const express = require('express');
const config = require('../config');
const { supabase, must, HttpError } = require('../db');
const V = require('../validate');
const { requireUser, generateDeviceKey, asyncH } = require('../auth');

const router = express.Router();
router.use(requireUser);

const DEVICE_FIELDS = [
  'id', 'owner_id', 'name', 'location', 'api_key_hint',
  'mode', 'manual_pump', 'soil_low', 'soil_high', 'max_pump_sec', 'cooldown_sec',
  'config_version', 'applied_version',
  'temperature', 'humidity', 'soil_moisture', 'soil_raw', 'pump_state',
  'firmware', 'local_ip', 'rssi', 'last_seen', 'created_at',
].join(', ');

// Bổ sung các trường tính toán cho giao diện.
function present(d) {
  const ageSec = d.last_seen ? (Date.now() - new Date(d.last_seen).getTime()) / 1000 : null;
  return {
    ...d,
    online: ageSec !== null && ageSec <= config.onlineWindowSec,
    seconds_since_seen: ageSec === null ? null : Math.round(ageSec),
    config_pending: d.applied_version < d.config_version, // web đã đổi nhưng ESP32 chưa nhận
  };
}

// Lấy thiết bị và kiểm tra quyền truy cập.
async function loadDevice(req, id) {
  V.uuid(id, 'Mã thiết bị');
  const d = must(await supabase.from('devices').select(DEVICE_FIELDS).eq('id', id).maybeSingle(), 'device');
  if (!d || (req.user.role !== 'admin' && d.owner_id !== req.user.id)) {
    throw new HttpError(404, 'Không tìm thấy thiết bị');
  }
  return d;
}

// ---------------- Danh sách & CRUD ----------------
router.get('/', asyncH(async (req, res) => {
  let q = supabase.from('devices').select(`${DEVICE_FIELDS}, owner:app_users(email, full_name)`).order('created_at');
  const all = req.user.role === 'admin' && req.query.all === '1';
  if (!all) q = q.eq('owner_id', req.user.id);
  const rows = must(await q, 'devices.list');
  res.json({ devices: rows.map(present) });
}));

router.post('/', asyncH(async (req, res) => {
  const name = V.text(req.body.name, { label: 'Tên thiết bị', max: 60, required: true });
  const location = V.text(req.body.location, { label: 'Vị trí', max: 100 });
  const { key, hash, hint } = generateDeviceKey();
  const device = must(
    await supabase
      .from('devices')
      .insert({ name, location, owner_id: req.user.id, api_key_hash: hash, api_key_hint: hint })
      .select(DEVICE_FIELDS)
      .single(),
    'devices.create'
  );
  // api_key chỉ trả về DUY NHẤT lần này.
  res.status(201).json({ device: present(device), api_key: key });
}));

router.get('/:id', asyncH(async (req, res) => {
  res.json({ device: present(await loadDevice(req, req.params.id)) });
}));

router.patch('/:id', asyncH(async (req, res) => {
  await loadDevice(req, req.params.id);
  const patch = {};
  if (req.body.name !== undefined) patch.name = V.text(req.body.name, { label: 'Tên thiết bị', max: 60, required: true });
  if (req.body.location !== undefined) patch.location = V.text(req.body.location, { label: 'Vị trí', max: 100 });
  if (req.body.owner_id !== undefined) {
    if (req.user.role !== 'admin') throw new HttpError(403, 'Chỉ quản trị viên được chuyển chủ sở hữu');
    const ownerId = req.body.owner_id === null ? null : V.uuid(req.body.owner_id, 'Người sở hữu');
    if (ownerId) {
      const u = must(await supabase.from('app_users').select('id').eq('id', ownerId).maybeSingle());
      if (!u) throw new HttpError(400, 'Người sở hữu không tồn tại');
    }
    patch.owner_id = ownerId;
  }
  const d = must(await supabase.from('devices').update(patch).eq('id', req.params.id).select(DEVICE_FIELDS).single());
  res.json({ device: present(d) });
}));

router.delete('/:id', asyncH(async (req, res) => {
  await loadDevice(req, req.params.id);
  must(await supabase.from('devices').delete().eq('id', req.params.id), 'devices.delete');
  res.json({ ok: true });
}));

router.post('/:id/regenerate-key', asyncH(async (req, res) => {
  await loadDevice(req, req.params.id);
  const { key, hash, hint } = generateDeviceKey();
  must(await supabase.from('devices').update({ api_key_hash: hash, api_key_hint: hint }).eq('id', req.params.id));
  res.json({ api_key: key });
}));

// ---------------- Điều khiển ----------------
// Đổi chế độ / bật tắt bơm / ngưỡng. Mỗi lần đổi tăng config_version,
// ESP32 thấy phiên bản mới ở lần đồng bộ kế tiếp (≤ vài giây) và áp dụng.
router.patch('/:id/settings', asyncH(async (req, res) => {
  const b = req.body || {};
  for (let attempt = 0; attempt < 3; attempt++) {
    const d = await loadDevice(req, req.params.id);
    const patch = {};
    if (b.mode !== undefined) {
      if (!['auto', 'manual'].includes(b.mode)) throw new HttpError(400, 'Chế độ phải là auto hoặc manual');
      patch.mode = b.mode;
    }
    if (b.manual_pump !== undefined) {
      patch.manual_pump = V.bool(b.manual_pump, 'Trạng thái bơm');
      // Bấm bật/tắt bơm khi đang AUTO → tự chuyển sang MANUAL để lệnh có hiệu lực.
      if (b.mode === undefined) patch.mode = 'manual';
    }
    // Chuyển về AUTO thì luôn tắt cờ bơm tay, tránh lần sau vào MANUAL bơm tự chạy.
    if (patch.mode === 'auto') patch.manual_pump = false;

    if (b.soil_low !== undefined) patch.soil_low = V.int(b.soil_low, { label: 'Ngưỡng bật bơm', min: 0, max: 100 });
    if (b.soil_high !== undefined) patch.soil_high = V.int(b.soil_high, { label: 'Ngưỡng tắt bơm', min: 0, max: 100 });
    if (b.max_pump_sec !== undefined) patch.max_pump_sec = V.int(b.max_pump_sec, { label: 'Thời gian bơm tối đa', min: 5, max: 3600 });
    if (b.cooldown_sec !== undefined) patch.cooldown_sec = V.int(b.cooldown_sec, { label: 'Thời gian nghỉ', min: 0, max: 86400 });

    const low = patch.soil_low ?? d.soil_low;
    const high = patch.soil_high ?? d.soil_high;
    if (low >= high) throw new HttpError(400, 'Ngưỡng bật bơm phải nhỏ hơn ngưỡng tắt bơm');
    if (!Object.keys(patch).length) throw new HttpError(400, 'Không có thay đổi nào');

    patch.config_version = d.config_version + 1;
    // Cập nhật có điều kiện theo phiên bản cũ (optimistic locking) để 2 người
    // bấm cùng lúc không ghi đè nhau. Nếu xung đột thì đọc lại và thử lại.
    const rows = must(
      await supabase.from('devices').update(patch)
        .eq('id', d.id).eq('config_version', d.config_version)
        .select(DEVICE_FIELDS),
      'devices.settings'
    );
    if (rows.length) return res.json({ device: present(rows[0]) });
  }
  throw new HttpError(409, 'Thiết bị vừa được thay đổi, vui lòng thử lại');
}));

// ---------------- Dữ liệu lịch sử ----------------
const RANGES = {
  '1h': { sec: 3600, bucket: 60 },
  '6h': { sec: 6 * 3600, bucket: 5 * 60 },
  '24h': { sec: 24 * 3600, bucket: 15 * 60 },
  '7d': { sec: 7 * 86400, bucket: 3600 },
  '30d': { sec: 30 * 86400, bucket: 4 * 3600 },
};

router.get('/:id/history', asyncH(async (req, res) => {
  const d = await loadDevice(req, req.params.id);
  const r = RANGES[req.query.range] || RANGES['24h'];
  const to = new Date();
  const from = new Date(to.getTime() - r.sec * 1000);
  const points = must(
    await supabase.rpc('readings_bucketed', {
      p_device: d.id, p_from: from.toISOString(), p_to: to.toISOString(), p_bucket_sec: r.bucket,
    }),
    'history'
  );
  res.json({ range: req.query.range || '24h', bucket_sec: r.bucket, points });
}));

router.get('/:id/events', asyncH(async (req, res) => {
  const d = await loadDevice(req, req.params.id);
  const limit = Math.min(200, Math.max(1, parseInt(req.query.limit, 10) || 50));
  const events = must(
    await supabase.from('pump_events').select('pump_on, source, created_at')
      .eq('device_id', d.id).order('created_at', { ascending: false }).limit(limit),
    'events'
  );
  res.json({ events });
}));

// Xuất CSV toàn bộ dữ liệu gốc trong khoảng thời gian (mở bằng Excel).
router.get('/:id/export.csv', asyncH(async (req, res) => {
  const d = await loadDevice(req, req.params.id);
  const r = RANGES[req.query.range] || RANGES['7d'];
  const from = new Date(Date.now() - r.sec * 1000).toISOString();
  const lines = ['thoi_gian,nhiet_do_C,do_am_kk_%,do_am_dat_%,bom'];
  const PAGE = 1000; // Supabase trả tối đa 1000 dòng / lần → lấy theo trang
  for (let offset = 0; offset < 100000; offset += PAGE) {
    const rows = must(
      await supabase.from('sensor_readings')
        .select('created_at, temperature, humidity, soil_moisture, pump_state')
        .eq('device_id', d.id).gte('created_at', from)
        .order('created_at').range(offset, offset + PAGE - 1),
      'export'
    );
    for (const x of rows) {
      lines.push([x.created_at, x.temperature ?? '', x.humidity ?? '', x.soil_moisture ?? '', x.pump_state ? 1 : 0].join(','));
    }
    if (rows.length < PAGE) break;
  }
  const safeName = d.name.replace(/[^\w-]+/g, '_') || 'device';
  res.setHeader('Content-Type', 'text/csv; charset=utf-8');
  res.setHeader('Content-Disposition', `attachment; filename="${safeName}_${req.query.range || '7d'}.csv"`);
  res.send('﻿' + lines.join('\n')); // BOM để Excel đọc đúng tiếng Việt
}));

module.exports = router;
module.exports.present = present;
module.exports.DEVICE_FIELDS = DEVICE_FIELDS;
