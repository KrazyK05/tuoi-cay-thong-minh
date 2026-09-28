// Đọc và kiểm tra biến môi trường một lần, dùng chung cho cả server.
require('dotenv').config();

function required(name) {
  const v = process.env[name];
  if (!v || !v.trim()) {
    console.error(`[CONFIG] Thiếu biến môi trường ${name}. Xem file .env.example`);
    process.exit(1);
  }
  return v.trim();
}

const config = {
  port: parseInt(process.env.PORT, 10) || 3000,
  supabaseUrl: required('SUPABASE_URL'),
  supabaseKey: required('SUPABASE_SERVICE_ROLE_KEY'),
  jwtSecret: required('JWT_SECRET'),
  jwtExpiresIn: process.env.JWT_EXPIRES_IN || '7d',
  allowRegister: (process.env.ALLOW_REGISTER || 'true').toLowerCase() !== 'false',
  retentionDays: Math.max(1, parseInt(process.env.RETENTION_DAYS, 10) || 90),
  onlineWindowSec: Math.max(10, parseInt(process.env.ONLINE_WINDOW_SEC, 10) || 30),
};

if (config.jwtSecret.length < 24) {
  console.warn('[CONFIG] JWT_SECRET quá ngắn — nên dùng chuỗi ngẫu nhiên ≥ 48 ký tự.');
}

module.exports = config;
