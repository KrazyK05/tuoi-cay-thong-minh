// Đăng ký, đăng nhập, thông tin cá nhân, đổi mật khẩu.
const express = require('express');
const rateLimit = require('express-rate-limit');
const config = require('../config');
const { supabase, must, HttpError } = require('../db');
const V = require('../validate');
const {
  USER_PUBLIC_FIELDS, hashPassword, checkPassword, signToken, requireUser, asyncH,
} = require('../auth');

const router = express.Router();

// Chống dò mật khẩu: tối đa 20 lần / 15 phút / IP cho đăng nhập & đăng ký.
const authLimiter = rateLimit({
  windowMs: 15 * 60 * 1000,
  limit: 20,
  standardHeaders: 'draft-7',
  legacyHeaders: false,
  message: { error: 'Thử quá nhiều lần, vui lòng đợi 15 phút' },
});

// Thông tin công khai cho trang đăng nhập (có cho đăng ký không).
router.get('/options', asyncH(async (req, res) => {
  const { count } = await supabase.from('app_users').select('id', { count: 'exact', head: true });
  res.json({ allow_register: config.allowRegister || !count, first_user: !count });
}));

router.post('/register', authLimiter, asyncH(async (req, res) => {
  const email = V.email(req.body.email);
  const password = V.password(req.body.password);
  const full_name = V.text(req.body.full_name, { label: 'Họ tên', max: 80, required: true });
  const phone = V.text(req.body.phone, { label: 'Số điện thoại', max: 20 }) || null;

  const { count } = await supabase.from('app_users').select('id', { count: 'exact', head: true });
  const isFirstUser = !count;
  if (!config.allowRegister && !isFirstUser) {
    throw new HttpError(403, 'Hệ thống không cho tự đăng ký. Liên hệ quản trị viên để được cấp tài khoản.');
  }

  const exists = must(await supabase.from('app_users').select('id').eq('email', email).maybeSingle());
  if (exists) throw new HttpError(409, 'Email đã được sử dụng');

  const user = must(
    await supabase
      .from('app_users')
      .insert({
        email,
        password_hash: await hashPassword(password),
        full_name,
        phone,
        role: isFirstUser ? 'admin' : 'user', // người đầu tiên là quản trị viên
      })
      .select(USER_PUBLIC_FIELDS)
      .single(),
    'register'
  );
  res.status(201).json({ token: signToken(user), user });
}));

router.post('/login', authLimiter, asyncH(async (req, res) => {
  const email = String(req.body.email || '').trim().toLowerCase();
  const password = String(req.body.password || '');

  const row = must(
    await supabase.from('app_users').select(`${USER_PUBLIC_FIELDS}, password_hash`).eq('email', email).maybeSingle()
  );
  // Cùng một thông báo cho cả 2 trường hợp để không lộ email nào tồn tại.
  if (!row || !(await checkPassword(password, row.password_hash))) {
    throw new HttpError(401, 'Email hoặc mật khẩu không đúng');
  }
  if (!row.is_active) throw new HttpError(403, 'Tài khoản đã bị khoá');

  const now = new Date().toISOString();
  await supabase.from('app_users').update({ last_login_at: now }).eq('id', row.id);
  const { password_hash, ...user } = row;
  user.last_login_at = now;
  res.json({ token: signToken(user), user });
}));

router.get('/me', requireUser, (req, res) => res.json({ user: req.user }));

router.patch('/me', requireUser, asyncH(async (req, res) => {
  const patch = {};
  if (req.body.full_name !== undefined) patch.full_name = V.text(req.body.full_name, { label: 'Họ tên', max: 80, required: true });
  if (req.body.phone !== undefined) patch.phone = V.text(req.body.phone, { label: 'Số điện thoại', max: 20 }) || null;
  const user = must(
    await supabase.from('app_users').update(patch).eq('id', req.user.id).select(USER_PUBLIC_FIELDS).single()
  );
  res.json({ user });
}));

router.post('/change-password', requireUser, asyncH(async (req, res) => {
  const newPw = V.password(req.body.new_password, 'Mật khẩu mới');
  const row = must(await supabase.from('app_users').select('password_hash').eq('id', req.user.id).single());
  if (!(await checkPassword(String(req.body.current_password || ''), row.password_hash))) {
    throw new HttpError(400, 'Mật khẩu hiện tại không đúng');
  }
  must(await supabase.from('app_users').update({ password_hash: await hashPassword(newPw) }).eq('id', req.user.id));
  res.json({ ok: true });
}));

module.exports = router;
