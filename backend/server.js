// =====================================================================
//  CLOUD SERVER — HỆ THỐNG TƯỚI CÂY THÔNG MINH IoT
//  - /api/device/*  : API cho ESP32 (xác thực bằng khoá thiết bị)
//  - /api/auth/*    : đăng ký, đăng nhập, tài khoản
//  - /api/devices/* : quản lý & điều khiển thiết bị (người dùng)
//  - /api/admin/*   : quản lý người dùng, thống kê (admin)
//  - /              : web dashboard (thư mục public/)
//  - /healthz       : kiểm tra sống (dùng cho UptimeRobot giữ server thức 24/7)
// =====================================================================
const path = require('path');
const express = require('express');
const config = require('./src/config');
const { supabase } = require('./src/db');

const app = express();
app.set('trust proxy', 1);           // Render đứng sau proxy → lấy đúng IP người dùng
app.disable('x-powered-by');
app.use(express.json({ limit: '64kb' }));

app.use((req, res, next) => {
  res.setHeader('X-Content-Type-Options', 'nosniff');
  res.setHeader('X-Frame-Options', 'DENY');
  res.setHeader('Referrer-Policy', 'same-origin');
  next();
});

// ---------- Kiểm tra sống ----------
const startedAt = Date.now();
app.get('/healthz', async (req, res) => {
  // Truy vấn nhẹ vào DB để Supabase cũng ghi nhận có hoạt động (tránh bị tạm dừng).
  const t0 = Date.now();
  const { error } = await supabase.from('devices').select('id', { head: true, count: 'estimated' }).limit(1);
  res.status(error ? 503 : 200).json({
    ok: !error,
    db: error ? error.message : 'ok',
    db_ms: Date.now() - t0,
    uptime_sec: Math.round((Date.now() - startedAt) / 1000),
  });
});

// ---------- API ----------
app.use('/api/device', require('./src/routes/deviceApi'));
app.use('/api/auth', require('./src/routes/auth'));
app.use('/api/devices', require('./src/routes/devices'));
app.use('/api/admin', require('./src/routes/admin'));
app.use('/api', (req, res) => res.status(404).json({ error: 'API không tồn tại' }));

// ---------- Web dashboard ----------
app.use(express.static(path.join(__dirname, 'public'), { maxAge: '1h' }));
app.get('*', (req, res) => res.sendFile(path.join(__dirname, 'public', 'index.html')));

// ---------- Xử lý lỗi ----------
// eslint-disable-next-line no-unused-vars
app.use((err, req, res, next) => {
  if (err.type === 'entity.parse.failed') return res.status(400).json({ error: 'JSON không hợp lệ' });
  const status = err.status || 500;
  if (status >= 500) console.error(`[ERROR] ${req.method} ${req.originalUrl}:`, err.message);
  res.status(status).json({ error: status >= 500 ? 'Lỗi máy chủ, vui lòng thử lại' : err.message });
});

// ---------- Dọn dữ liệu cũ (mỗi 6 giờ) ----------
async function cleanupOldData() {
  const cutoff = new Date(Date.now() - config.retentionDays * 86400 * 1000).toISOString();
  const r1 = await supabase.from('sensor_readings').delete().lt('created_at', cutoff);
  const r2 = await supabase.from('pump_events').delete().lt('created_at', cutoff);
  if (r1.error || r2.error) console.error('[CLEANUP]', (r1.error || r2.error).message);
  else console.log(`[CLEANUP] Đã xoá dữ liệu trước ${cutoff}`);
}

if (require.main === module) {
  app.listen(config.port, () => {
    console.log(`[SERVER] Đang chạy tại cổng ${config.port}`);
    setTimeout(cleanupOldData, 60 * 1000);
    setInterval(cleanupOldData, 6 * 3600 * 1000);
  });
}

module.exports = app;
