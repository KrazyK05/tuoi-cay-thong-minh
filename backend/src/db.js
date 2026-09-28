// Kết nối Supabase bằng khoá service_role (bỏ qua RLS) — CHỈ dùng phía server.
const { createClient } = require('@supabase/supabase-js');
const config = require('./config');

const supabase = createClient(config.supabaseUrl, config.supabaseKey, {
  auth: { persistSession: false, autoRefreshToken: false },
});

// Lỗi có mã HTTP, được middleware xử lý lỗi trả về cho client.
class HttpError extends Error {
  constructor(status, message) {
    super(message);
    this.status = status;
  }
}

// Ném lỗi nếu truy vấn Supabase thất bại, ngược lại trả về data.
function must({ data, error }, context = 'db') {
  if (error) {
    const err = new Error(`[${context}] ${error.message}`);
    err.status = 500;
    err.pg = error;
    throw err;
  }
  return data;
}

module.exports = { supabase, must, HttpError };
