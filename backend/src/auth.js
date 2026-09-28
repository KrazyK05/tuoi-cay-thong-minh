// Xác thực người dùng (JWT) và xác thực thiết bị (API key).
const crypto = require('crypto');
const bcrypt = require('bcryptjs');
const jwt = require('jsonwebtoken');
const config = require('./config');
const { supabase, must, HttpError } = require('./db');

const USER_PUBLIC_FIELDS = 'id, email, full_name, phone, role, is_active, last_login_at, created_at';

// ---------- Mật khẩu ----------
const hashPassword = (plain) => bcrypt.hash(plain, 10);
const checkPassword = (plain, hash) => bcrypt.compare(plain, hash);

// ---------- Token đăng nhập ----------
function signToken(user) {
  return jwt.sign({ sub: user.id, role: user.role }, config.jwtSecret, {
    expiresIn: config.jwtExpiresIn,
  });
}

// Middleware: bắt buộc đăng nhập. Luôn đọc lại user từ DB để việc
// khoá tài khoản / đổi quyền có hiệu lực ngay, không đợi token hết hạn.
const asyncH = (fn) => (req, res, next) => Promise.resolve(fn(req, res, next)).catch(next);

const requireUser = asyncH(async (req, res, next) => {
  const header = req.get('authorization') || '';
  const token = header.startsWith('Bearer ') ? header.slice(7) : null;
  if (!token) throw new HttpError(401, 'Chưa đăng nhập');

  let payload;
  try {
    payload = jwt.verify(token, config.jwtSecret);
  } catch {
    throw new HttpError(401, 'Phiên đăng nhập hết hạn, vui lòng đăng nhập lại');
  }

  const user = must(
    await supabase.from('app_users').select(USER_PUBLIC_FIELDS).eq('id', payload.sub).maybeSingle(),
    'auth.user'
  );
  if (!user) throw new HttpError(401, 'Tài khoản không tồn tại');
  if (!user.is_active) throw new HttpError(403, 'Tài khoản đã bị khoá');
  req.user = user;
  next();
});

function requireAdmin(req, res, next) {
  if (!req.user || req.user.role !== 'admin') {
    return next(new HttpError(403, 'Chỉ quản trị viên được phép'));
  }
  next();
}

// ---------- Khoá thiết bị ----------
// Khoá gốc chỉ hiển thị 1 lần khi tạo; DB chỉ lưu SHA-256.
function generateDeviceKey() {
  const key = 'dev_' + crypto.randomBytes(20).toString('hex'); // 44 ký tự
  return { key, hash: hashDeviceKey(key), hint: key.slice(-4) };
}
const hashDeviceKey = (key) => crypto.createHash('sha256').update(key).digest('hex');

module.exports = {
  USER_PUBLIC_FIELDS,
  hashPassword,
  checkPassword,
  signToken,
  requireUser,
  requireAdmin,
  generateDeviceKey,
  hashDeviceKey,
  asyncH,
};
