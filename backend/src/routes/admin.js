// Quản lý người dùng & thống kê hệ thống — chỉ dành cho admin.
const express = require('express');
const config = require('../config');
const { supabase, must, HttpError } = require('../db');
const V = require('../validate');
const { USER_PUBLIC_FIELDS, hashPassword, requireUser, requireAdmin, asyncH } = require('../auth');

const router = express.Router();
router.use(requireUser, requireAdmin);

router.get('/stats', asyncH(async (req, res) => {
  const onlineSince = new Date(Date.now() - config.onlineWindowSec * 1000).toISOString();
  const dayAgo = new Date(Date.now() - 86400 * 1000).toISOString();
  const count = async (q) => (await q).count || 0;
  const [users, devices, online, pumping, readings24h] = await Promise.all([
    count(supabase.from('app_users').select('id', { count: 'exact', head: true })),
    count(supabase.from('devices').select('id', { count: 'exact', head: true })),
    count(supabase.from('devices').select('id', { count: 'exact', head: true }).gte('last_seen', onlineSince)),
    count(supabase.from('devices').select('id', { count: 'exact', head: true }).gte('last_seen', onlineSince).eq('pump_state', true)),
    count(supabase.from('sensor_readings').select('id', { count: 'exact', head: true }).gte('created_at', dayAgo)),
  ]);
  res.json({ users, devices, online, pumping, readings24h, retention_days: config.retentionDays });
}));

router.get('/users', asyncH(async (req, res) => {
  const users = must(
    await supabase.from('app_users').select(`${USER_PUBLIC_FIELDS}, devices(count)`).order('created_at'),
    'admin.users'
  );
  res.json({
    users: users.map(({ devices, ...u }) => ({ ...u, device_count: devices?.[0]?.count ?? 0 })),
  });
}));

router.post('/users', asyncH(async (req, res) => {
  const email = V.email(req.body.email);
  const password = V.password(req.body.password);
  const full_name = V.text(req.body.full_name, { label: 'Họ tên', max: 80, required: true });
  const phone = V.text(req.body.phone, { label: 'Số điện thoại', max: 20 }) || null;
  const role = req.body.role === 'admin' ? 'admin' : 'user';

  const exists = must(await supabase.from('app_users').select('id').eq('email', email).maybeSingle());
  if (exists) throw new HttpError(409, 'Email đã được sử dụng');
  const user = must(
    await supabase.from('app_users')
      .insert({ email, password_hash: await hashPassword(password), full_name, phone, role })
      .select(USER_PUBLIC_FIELDS).single(),
    'admin.createUser'
  );
  res.status(201).json({ user });
}));

// Không cho hệ thống rơi vào tình trạng không còn admin nào hoạt động.
async function ensureAnotherActiveAdmin(excludeId) {
  const { count } = await supabase.from('app_users').select('id', { count: 'exact', head: true })
    .eq('role', 'admin').eq('is_active', true).neq('id', excludeId);
  if (!count) throw new HttpError(400, 'Phải còn ít nhất một quản trị viên đang hoạt động');
}

router.patch('/users/:id', asyncH(async (req, res) => {
  const id = V.uuid(req.params.id);
  const target = must(await supabase.from('app_users').select('id, role, is_active').eq('id', id).maybeSingle());
  if (!target) throw new HttpError(404, 'Không tìm thấy người dùng');

  const patch = {};
  if (req.body.full_name !== undefined) patch.full_name = V.text(req.body.full_name, { label: 'Họ tên', max: 80, required: true });
  if (req.body.phone !== undefined) patch.phone = V.text(req.body.phone, { label: 'Số điện thoại', max: 20 }) || null;
  if (req.body.role !== undefined) {
    if (!['admin', 'user'].includes(req.body.role)) throw new HttpError(400, 'Vai trò không hợp lệ');
    patch.role = req.body.role;
  }
  if (req.body.is_active !== undefined) patch.is_active = V.bool(req.body.is_active, 'Trạng thái');

  const losesAdmin = target.role === 'admin' && target.is_active &&
    (patch.role === 'user' || patch.is_active === false);
  if (losesAdmin) await ensureAnotherActiveAdmin(id);

  const user = must(await supabase.from('app_users').update(patch).eq('id', id).select(USER_PUBLIC_FIELDS).single());
  res.json({ user });
}));

router.post('/users/:id/reset-password', asyncH(async (req, res) => {
  const id = V.uuid(req.params.id);
  const pw = V.password(req.body.new_password, 'Mật khẩu mới');
  const rows = must(await supabase.from('app_users').update({ password_hash: await hashPassword(pw) }).eq('id', id).select('id'));
  if (!rows.length) throw new HttpError(404, 'Không tìm thấy người dùng');
  res.json({ ok: true });
}));

router.delete('/users/:id', asyncH(async (req, res) => {
  const id = V.uuid(req.params.id);
  if (id === req.user.id) throw new HttpError(400, 'Không thể tự xoá tài khoản của mình');
  const target = must(await supabase.from('app_users').select('id, role').eq('id', id).maybeSingle());
  if (!target) throw new HttpError(404, 'Không tìm thấy người dùng');
  if (target.role === 'admin') await ensureAnotherActiveAdmin(id);
  // Thiết bị của người này KHÔNG bị xoá, chỉ chuyển thành "chưa có chủ" (owner_id = NULL).
  must(await supabase.from('app_users').delete().eq('id', id), 'admin.deleteUser');
  res.json({ ok: true });
}));

module.exports = router;
