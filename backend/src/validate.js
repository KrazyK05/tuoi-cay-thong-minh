// Các hàm kiểm tra dữ liệu đầu vào dùng chung.
const { HttpError } = require('./db');

const EMAIL_RE = /^[^\s@]+@[^\s@]+\.[^\s@]{2,}$/;

function email(v) {
  const e = String(v || '').trim().toLowerCase();
  if (!EMAIL_RE.test(e) || e.length > 254) throw new HttpError(400, 'Email không hợp lệ');
  return e;
}

function password(v, label = 'Mật khẩu') {
  const p = String(v || '');
  if (p.length < 6) throw new HttpError(400, `${label} phải có ít nhất 6 ký tự`);
  if (p.length > 72) throw new HttpError(400, `${label} tối đa 72 ký tự`);
  return p;
}

function text(v, { label, max = 100, required = false } = {}) {
  const s = String(v ?? '').trim();
  if (required && !s) throw new HttpError(400, `${label} không được để trống`);
  if (s.length > max) throw new HttpError(400, `${label} tối đa ${max} ký tự`);
  return s;
}

function int(v, { label, min, max }) {
  const n = Number(v);
  if (!Number.isInteger(n) || n < min || n > max) {
    throw new HttpError(400, `${label} phải là số nguyên từ ${min} đến ${max}`);
  }
  return n;
}

function bool(v, label) {
  if (typeof v !== 'boolean') throw new HttpError(400, `${label} phải là true/false`);
  return v;
}

// Số thực hợp lệ trong khoảng, ngược lại trả null (dùng cho dữ liệu cảm biến).
function num(v, min, max) {
  const n = Number(v);
  return v !== null && v !== undefined && v !== '' && Number.isFinite(n) && n >= min && n <= max
    ? n
    : null;
}

const UUID_RE = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;
function uuid(v, label = 'ID') {
  if (!UUID_RE.test(String(v))) throw new HttpError(400, `${label} không hợp lệ`);
  return String(v).toLowerCase();
}

module.exports = { email, password, text, int, bool, num, uuid };
