/* =====================================================================
   WEB DASHBOARD — IoTFarm · Hệ thống giám sát cây trồng
   Ứng dụng 1 trang (SPA) viết bằng JavaScript thuần, điều hướng bằng #hash.
   ===================================================================== */
'use strict';

const $ = (sel, root = document) => root.querySelector(sel);
const $$ = (sel, root = document) => [...root.querySelectorAll(sel)];
const app = $('#app');

const state = {
  token: localStorage.getItem('token'),
  user: null,
  timers: [],
  chart: null,
  range: '24h',
  authOptions: null,
};

// ---------------- Tiện ích ----------------
const esc = (s) => String(s ?? '').replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
const fmt = (v, digits = 1) => (v === null || v === undefined || v === '' ? '--' : Number(v).toFixed(digits));
const fmtTime = (iso) => (iso ? new Date(iso).toLocaleString('vi-VN', { hour12: false }) : '—');

function timeAgo(sec) {
  if (sec === null || sec === undefined) return 'chưa kết nối lần nào';
  if (sec < 60) return `${sec} giây trước`;
  if (sec < 3600) return `${Math.floor(sec / 60)} phút trước`;
  if (sec < 86400) return `${Math.floor(sec / 3600)} giờ trước`;
  return `${Math.floor(sec / 86400)} ngày trước`;
}

function toast(msg, isErr = false) {
  const t = $('#toast');
  t.textContent = msg;
  t.className = 'toast show' + (isErr ? ' err' : '');
  clearTimeout(toast._t);
  toast._t = setTimeout(() => (t.className = 'toast'), 2800);
}

function every(ms, fn) {
  const id = setInterval(fn, ms);
  state.timers.push(id);
}
function clearTimers() {
  state.timers.forEach(clearInterval);
  state.timers = [];
  if (state.chart) { state.chart.destroy(); state.chart = null; }
}

async function api(method, path, body) {
  const res = await fetch('/api' + path, {
    method,
    headers: {
      'Content-Type': 'application/json',
      ...(state.token ? { Authorization: 'Bearer ' + state.token } : {}),
    },
    body: body === undefined ? undefined : JSON.stringify(body),
  });
  const data = await res.json().catch(() => ({}));
  if (res.status === 401 && state.token) {
    logout(false);
    throw new Error(data.error || 'Phiên đăng nhập hết hạn');
  }
  if (!res.ok) throw new Error(data.error || `Lỗi ${res.status}`);
  return data;
}

// Chạy hàm async, tự hiện lỗi lên toast và khoá nút trong lúc chờ.
async function run(btn, fn) {
  if (btn) btn.disabled = true;
  try { return await fn(); }
  catch (e) { toast(e.message, true); }
  finally { if (btn) btn.disabled = false; }
}

const ICON = {
  drop: '<svg viewBox="0 0 32 32" aria-hidden="true"><path d="M16 3C11 10 7 14.5 7 19a9 9 0 0 0 18 0c0-4.5-4-9-9-16z"/></svg>',
  power: '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M11 2h2v10h-2zM6.3 5.3l1.4 1.4A7 7 0 1 0 16.3 6.7l1.4-1.4A9 9 0 1 1 6.3 5.3z"/></svg>',
};

// ---------------- Modal ----------------
function openModal(html, onSubmit) {
  const dlg = $('#modal');
  const body = $('#modalBody');
  body.innerHTML = html;
  body.onsubmit = async (e) => {
    const action = e.submitter?.value;
    if (action === 'cancel' || !onSubmit) return; // đóng bình thường
    e.preventDefault();
    const btn = e.submitter;
    await run(btn, async () => {
      const close = await onSubmit(new FormData(body), action);
      if (close !== false) dlg.close();
    });
  };
  dlg.showModal();
  const first = body.querySelector('input, select');
  if (first) first.focus();
}

function showKeyModal(deviceName, key) {
  openModal(`
    <h2>Khoá thiết bị “${esc(deviceName)}”</h2>
    <div class="notice warn">Khoá chỉ hiển thị <b>một lần duy nhất</b>. Hãy sao chép và dán vào firmware ngay.</div>
    <div class="keybox"><code id="keyText">${esc(key)}</code>
      <button type="button" class="btn sm" id="btnCopyKey">Sao chép</button></div>
    <p class="small muted" style="margin:12px 0 0">Dán vào file <b>include/app_config.h</b> của firmware ESP32:</p>
    <pre class="snippet">#define DEVICE_KEY "${esc(key)}"</pre>
    <div class="modal-actions"><button class="btn" value="cancel">Tôi đã lưu khoá</button></div>
  `);
  $('#btnCopyKey').onclick = async () => {
    try { await navigator.clipboard.writeText(key); toast('Đã sao chép khoá'); }
    catch { toast('Không sao chép được, hãy bôi đen và copy thủ công', true); }
  };
}

// ---------------- Đăng nhập / đăng ký ----------------
function logout(showMsg = true) {
  state.token = null;
  state.user = null;
  localStorage.removeItem('token');
  if (showMsg) toast('Đã đăng xuất');
  location.hash = '#/login';
}

function authShell(inner) {
  return `<div class="auth-wrap"><div class="card auth-card">
    <svg class="logo" viewBox="0 0 32 32"><path d="M16 3C11 10 7 14.5 7 19a9 9 0 0 0 18 0c0-4.5-4-9-9-16z"/></svg>
    ${inner}</div></div>`;
}

async function viewLogin() {
  const opt = state.authOptions || (state.authOptions = await api('GET', '/auth/options').catch(() => ({})));
  app.innerHTML = authShell(`
    <h1>Đăng nhập</h1>
    <p class="muted">IoTFarm · Hệ thống giám sát cây trồng</p>
    ${opt.first_user ? '<div class="notice info">Hệ thống chưa có tài khoản nào. Tài khoản đăng ký đầu tiên sẽ là <b>quản trị viên</b>.</div>' : ''}
    <form id="f">
      <div class="field"><label for="email">Email</label><input id="email" name="email" type="email" autocomplete="username" required></div>
      <div class="field"><label for="password">Mật khẩu</label><input id="password" name="password" type="password" autocomplete="current-password" required></div>
      <button class="btn block">Đăng nhập</button>
    </form>
    ${opt.allow_register ? '<p class="small muted" style="text-align:center;margin:16px 0 0">Chưa có tài khoản? <a href="#/register">Đăng ký</a></p>' : ''}
  `);
  $('#f').onsubmit = (e) => {
    e.preventDefault();
    const fd = new FormData(e.target);
    run(e.submitter, async () => {
      const r = await api('POST', '/auth/login', { email: fd.get('email'), password: fd.get('password') });
      onLoggedIn(r);
    });
  };
}

async function viewRegister() {
  app.innerHTML = authShell(`
    <h1>Tạo tài khoản</h1>
    <p class="muted">Quản lý vườn của bạn từ bất cứ đâu</p>
    <form id="f">
      <div class="field"><label for="full_name">Họ và tên</label><input id="full_name" name="full_name" required maxlength="80"></div>
      <div class="field"><label for="email">Email</label><input id="email" name="email" type="email" autocomplete="username" required></div>
      <div class="field"><label for="phone">Số điện thoại <span class="muted">(không bắt buộc)</span></label><input id="phone" name="phone" maxlength="20"></div>
      <div class="field"><label for="password">Mật khẩu</label><input id="password" name="password" type="password" minlength="6" autocomplete="new-password" required>
        <div class="hint">Ít nhất 6 ký tự</div></div>
      <button class="btn block">Đăng ký</button>
    </form>
    <p class="small muted" style="text-align:center;margin:16px 0 0">Đã có tài khoản? <a href="#/login">Đăng nhập</a></p>
  `);
  $('#f').onsubmit = (e) => {
    e.preventDefault();
    const fd = Object.fromEntries(new FormData(e.target));
    run(e.submitter, async () => {
      const r = await api('POST', '/auth/register', fd);
      state.authOptions = null;
      onLoggedIn(r);
      toast(r.user.role === 'admin' ? 'Chào mừng! Bạn là quản trị viên hệ thống.' : 'Đăng ký thành công');
    });
  };
}

function onLoggedIn({ token, user }) {
  state.token = token;
  state.user = user;
  localStorage.setItem('token', token);
  location.hash = '#/devices';
}

// ---------------- Danh sách thiết bị ----------------
function deviceCard(d, showOwner) {
  return `<a class="card dev-card" href="#/device/${d.id}">
    <div class="row between">
      <div><div class="title">${esc(d.name)}</div><div class="small muted">${esc(d.location || 'Chưa đặt vị trí')}</div></div>
      <span class="badge ${d.online ? 'on' : 'off'}">${d.online ? 'Trực tuyến' : 'Ngoại tuyến'}</span>
    </div>
    <div class="metrics">
      <div class="metric-mini"><div class="v num">${fmt(d.temperature)}°</div><div class="k">Nhiệt độ</div></div>
      <div class="metric-mini"><div class="v num">${fmt(d.humidity, 0)}%</div><div class="k">Độ ẩm KK</div></div>
      <div class="metric-mini"><div class="v num">${fmt(d.soil_moisture, 0)}%</div><div class="k">Độ ẩm đất</div></div>
    </div>
    <div class="row between small">
      <span class="row" style="gap:6px">
        <span class="badge plain ${d.mode === 'auto' ? 'on' : 'warn'}">${d.mode === 'auto' ? 'Tự động' : 'Thủ công'}</span>
        <span class="badge ${d.pump_state ? 'water' : 'off'}">Bơm ${d.pump_state ? 'đang chạy' : 'tắt'}</span>
      </span>
      <span class="muted">${showOwner ? esc(d.owner?.email || 'chưa có chủ') : timeAgo(d.seconds_since_seen)}</span>
    </div>
  </a>`;
}

async function viewDevices() {
  const isAdmin = state.user.role === 'admin';
  const showAll = isAdmin && sessionStorage.getItem('showAll') === '1';
  app.innerHTML = `
    <div class="page-head">
      <div><h1>Thiết bị của ${showAll ? 'hệ thống' : 'tôi'}</h1><p class="muted" id="summary">Đang tải…</p></div>
      <div class="row">
        ${isAdmin ? `<label class="row small" style="gap:6px;margin:0;font-weight:500"><input type="checkbox" id="chkAll" style="width:auto" ${showAll ? 'checked' : ''}> Xem tất cả</label>` : ''}
        <button class="btn" id="btnAdd">+ Thêm thiết bị</button>
      </div>
    </div>
    <div id="list" class="dev-grid"><div class="loading">Đang tải…</div></div>`;

  $('#btnAdd').onclick = () => openModal(`
    <h2>Thêm thiết bị mới</h2>
    <div class="field"><label for="n">Tên thiết bị</label><input id="n" name="name" required maxlength="60" placeholder="VD: Vườn rau sân thượng"></div>
    <div class="field"><label for="l">Vị trí</label><input id="l" name="location" maxlength="100" placeholder="VD: Tầng 4, nhà A"></div>
    <div class="modal-actions"><button class="btn ghost" value="cancel" formnovalidate>Huỷ</button><button class="btn" value="ok">Tạo thiết bị</button></div>
  `, async (fd) => {
    const r = await api('POST', '/devices', { name: fd.get('name'), location: fd.get('location') });
    setTimeout(() => showKeyModal(r.device.name, r.api_key), 50);
    load();
  });

  if (isAdmin) $('#chkAll').onchange = (e) => { sessionStorage.setItem('showAll', e.target.checked ? '1' : '0'); viewDevices(); };

  async function load() {
    const { devices } = await api('GET', '/devices' + (showAll ? '?all=1' : ''));
    const online = devices.filter((d) => d.online).length;
    $('#summary').textContent = `${devices.length} thiết bị · ${online} trực tuyến`;
    $('#list').innerHTML = devices.length
      ? devices.map((d) => deviceCard(d, showAll)).join('')
      : `<div class="card empty" style="grid-column:1/-1">${ICON.drop}
          <h2 style="margin-top:8px">Chưa có thiết bị nào</h2>
          <p class="muted">Bấm “Thêm thiết bị” để lấy khoá kết nối cho ESP32.</p></div>`;
  }
  await load();
  every(5000, () => load().catch(() => {}));
}

// ---------------- Chi tiết thiết bị ----------------
async function viewDevice(id) {
  app.innerHTML = '<div class="loading">Đang tải…</div>';
  let { device: d } = await api('GET', `/devices/${id}`);
  let formDirty = false;

  app.innerHTML = `
    <div class="page-head">
      <div>
        <a href="#/devices" class="small">← Danh sách thiết bị</a>
        <h1 id="dName" style="margin-top:6px"></h1>
        <p class="muted" id="dSub"></p>
      </div>
      <span class="badge" id="dOnline"></span>
    </div>

    <div class="stats">
      <div class="card stat"><div class="k">Nhiệt độ</div><div class="v num" id="sT"></div><div class="sub">Cảm biến DHT11</div></div>
      <div class="card stat"><div class="k">Độ ẩm không khí</div><div class="v num" id="sH"></div><div class="sub">Cảm biến DHT11</div></div>
      <div class="card stat"><div class="k">Độ ẩm đất</div><div class="v num" id="sS"></div>
        <div class="bar" title="Vạch = ngưỡng bật / tắt bơm"><i id="sSBar"></i><b id="sLow"></b><b id="sHigh"></b></div>
        <div class="sub" id="sSraw"></div></div>
      <div class="card stat"><div class="k">Máy bơm</div><div class="v" id="sP"></div><div class="sub" id="sPsub"></div></div>
    </div>

    <div class="detail-grid">
      <div>
        <div class="card">
          <div class="card-head"><h2>Biểu đồ</h2>
            <div class="row">
              <div class="range-tabs" id="ranges">
                ${['1h', '6h', '24h', '7d', '30d'].map((r) => `<button type="button" data-r="${r}">${r.replace('h', ' giờ').replace('d', ' ngày')}</button>`).join('')}
              </div>
              <button class="btn ghost sm" id="btnCsv" type="button">Tải CSV</button>
            </div>
          </div>
          <div class="chart-box"><canvas id="chart"></canvas><div class="chart-empty" id="chartEmpty" hidden>Chưa có dữ liệu trong khoảng thời gian này</div></div>
        </div>
        <div class="card">
          <div class="card-head"><h2>Nhật ký bơm</h2><span class="small muted">50 lần gần nhất</span></div>
          <ul class="events" id="events"><li class="muted">Đang tải…</li></ul>
        </div>
      </div>

      <div>
        <div class="card">
          <div class="card-head"><h2>Điều khiển</h2></div>
          <div class="seg" id="modeSeg">
            <button type="button" data-mode="auto">Tự động</button>
            <button type="button" data-mode="manual">Thủ công</button>
          </div>
          <p class="small muted" id="modeHelp" style="margin:10px 0 0"></p>
          <button class="pump-btn" id="pumpBtn" type="button"></button>
          <div id="pendingNote" class="notice warn small" style="margin:12px 0 0" hidden></div>
        </div>

        <div class="card">
          <div class="card-head"><h2>Cài đặt tưới tự động</h2></div>
          <form id="setForm">
            <div class="grid2">
              <div class="field"><label for="soil_low">Bật bơm khi đất &lt;</label><input id="soil_low" name="soil_low" type="number" min="0" max="99" required><div class="hint">% độ ẩm đất</div></div>
              <div class="field"><label for="soil_high">Tắt bơm khi đất ≥</label><input id="soil_high" name="soil_high" type="number" min="1" max="100" required><div class="hint">% độ ẩm đất</div></div>
              <div class="field"><label for="max_pump_sec">Bơm tối đa mỗi lần</label><input id="max_pump_sec" name="max_pump_sec" type="number" min="5" max="3600" required><div class="hint">giây — tự ngắt để an toàn</div></div>
              <div class="field"><label for="cooldown_sec">Nghỉ giữa 2 lần tưới</label><input id="cooldown_sec" name="cooldown_sec" type="number" min="0" max="86400" required><div class="hint">giây — cho nước kịp thấm</div></div>
            </div>
            <button class="btn block" id="btnSave">Lưu cài đặt</button>
          </form>
        </div>

        <div class="card">
          <div class="card-head"><h2>Thông tin thiết bị</h2></div>
          <dl class="kv" id="info"></dl>
          <div class="row" style="margin-top:14px;gap:8px">
            <button class="btn ghost sm" id="btnEdit" type="button">Sửa tên / vị trí</button>
            <button class="btn ghost sm" id="btnKey" type="button">Tạo khoá mới</button>
            <button class="btn danger ghost sm" id="btnDel" type="button">Xoá thiết bị</button>
          </div>
        </div>
      </div>
    </div>`;

  // ---- Cập nhật phần hiển thị động ----
  function paint() {
    $('#dName').textContent = d.name;
    $('#dSub').textContent = `${d.location || 'Chưa đặt vị trí'} · cập nhật ${timeAgo(d.seconds_since_seen)}`;
    const on = $('#dOnline');
    on.className = 'badge ' + (d.online ? 'on' : 'off');
    on.textContent = d.online ? 'Trực tuyến' : 'Ngoại tuyến';

    $('#sT').innerHTML = `${fmt(d.temperature)}<small>°C</small>`;
    $('#sH').innerHTML = `${fmt(d.humidity, 0)}<small>%</small>`;
    $('#sS').innerHTML = `${fmt(d.soil_moisture, 0)}<small>%</small>`;
    $('#sSBar').style.width = `${Math.max(0, Math.min(100, d.soil_moisture ?? 0))}%`;
    $('#sLow').style.left = `${d.soil_low}%`;
    $('#sHigh').style.left = `${d.soil_high}%`;
    $('#sSraw').textContent = `ADC thô: ${d.soil_raw ?? '--'} · ngưỡng ${d.soil_low}–${d.soil_high}%`;
    $('#sP').innerHTML = d.pump_state
      ? '<span style="color:var(--water)">Đang bơm</span>'
      : '<span style="color:var(--off)">Tắt</span>';
    $('#sPsub').textContent = d.mode === 'auto' ? 'Chế độ tự động' : 'Chế độ thủ công';

    $$('#modeSeg button').forEach((b) => b.classList.toggle('active', b.dataset.mode === d.mode));
    $('#modeHelp').textContent = d.mode === 'auto'
      ? `Bơm tự bật khi đất < ${d.soil_low}% và tắt khi đất ≥ ${d.soil_high}% (tối đa ${d.max_pump_sec} giây/lần).`
      : 'Bạn bật/tắt bơm bằng nút bên dưới. Bơm vẫn tự ngắt sau thời gian tối đa để an toàn.';

    const pb = $('#pumpBtn');
    const wantOn = d.mode === 'manual' && d.manual_pump;
    pb.className = 'pump-btn' + (d.pump_state ? ' running' : '');
    pb.innerHTML = d.mode === 'auto'
      ? `${ICON.power} ${d.pump_state ? 'Tắt bơm & chuyển thủ công' : 'Bật bơm & chuyển thủ công'}`
      : `${ICON.power} ${wantOn ? 'TẮT BƠM' : 'BẬT BƠM'}`;

    const note = $('#pendingNote');
    if (!d.online) {
      note.hidden = false;
      note.textContent = d.config_pending
        ? 'Thiết bị đang ngoại tuyến. Lệnh sẽ được áp dụng khi thiết bị kết nối lại.'
        : 'Thiết bị đang ngoại tuyến. ESP32 vẫn tự tưới theo cài đặt đã lưu trong máy.';
    } else if (d.config_pending) {
      note.hidden = false;
      note.textContent = 'Đang gửi lệnh xuống thiết bị…';
    } else note.hidden = true;

    if (!formDirty) {
      for (const k of ['soil_low', 'soil_high', 'max_pump_sec', 'cooldown_sec']) $('#' + k).value = d[k];
    }

    const rssi = d.rssi == null ? '--' : `${d.rssi} dBm (${d.rssi > -60 ? 'mạnh' : d.rssi > -75 ? 'khá' : 'yếu'})`;
    $('#info').innerHTML = `
      <dt>Mã thiết bị</dt><dd class="mono">${esc(d.id.slice(0, 8))}…</dd>
      <dt>Khoá (4 ký tự cuối)</dt><dd class="mono">…${esc(d.api_key_hint)}</dd>
      <dt>Firmware</dt><dd>${esc(d.firmware || '--')}</dd>
      <dt>IP trong mạng LAN</dt><dd>${d.local_ip ? `<a href="http://${esc(d.local_ip)}" target="_blank" rel="noopener">${esc(d.local_ip)}</a>` : '--'}</dd>
      <dt>Sóng WiFi</dt><dd>${rssi}</dd>
      <dt>Lần cuối kết nối</dt><dd>${fmtTime(d.last_seen)}</dd>
      <dt>Ngày tạo</dt><dd>${fmtTime(d.created_at)}</dd>`;
  }

  async function refresh() {
    ({ device: d } = await api('GET', `/devices/${id}`));
    paint();
  }

  async function setSettings(patch, btn) {
    await run(btn, async () => {
      ({ device: d } = await api('PATCH', `/devices/${id}/settings`, patch));
      paint();
      toast(d.online ? 'Đã gửi lệnh' : 'Đã lưu — thiết bị sẽ nhận khi kết nối lại');
    });
  }

  // ---- Sự kiện ----
  $$('#modeSeg button').forEach((b) => (b.onclick = () => b.dataset.mode !== d.mode && setSettings({ mode: b.dataset.mode }, b)));
  $('#pumpBtn').onclick = (e) => {
    const next = d.mode === 'auto' ? !d.pump_state : !d.manual_pump;
    setSettings({ manual_pump: next }, e.currentTarget);
  };
  $('#setForm').oninput = () => (formDirty = true);
  $('#setForm').onsubmit = async (e) => {
    e.preventDefault();
    const fd = new FormData(e.target);
    const patch = Object.fromEntries(['soil_low', 'soil_high', 'max_pump_sec', 'cooldown_sec'].map((k) => [k, parseInt(fd.get(k), 10)]));
    if (patch.soil_low >= patch.soil_high) return toast('Ngưỡng bật phải nhỏ hơn ngưỡng tắt', true);
    formDirty = false;
    await setSettings(patch, $('#btnSave'));
  };

  $('#btnEdit').onclick = () => openModal(`
    <h2>Sửa thông tin thiết bị</h2>
    <div class="field"><label for="n">Tên thiết bị</label><input id="n" name="name" required maxlength="60" value="${esc(d.name)}"></div>
    <div class="field"><label for="l">Vị trí</label><input id="l" name="location" maxlength="100" value="${esc(d.location)}"></div>
    <div class="modal-actions"><button class="btn ghost" value="cancel" formnovalidate>Huỷ</button><button class="btn" value="ok">Lưu</button></div>
  `, async (fd) => {
    ({ device: d } = await api('PATCH', `/devices/${id}`, { name: fd.get('name'), location: fd.get('location') }));
    paint();
    toast('Đã lưu');
  });

  $('#btnKey').onclick = () => openModal(`
    <h2>Tạo khoá mới?</h2>
    <p>Khoá cũ sẽ <b>ngừng hoạt động ngay</b>. Bạn cần nạp lại firmware ESP32 với khoá mới.</p>
    <div class="modal-actions"><button class="btn ghost" value="cancel">Huỷ</button><button class="btn danger" value="ok">Tạo khoá mới</button></div>
  `, async () => {
    const r = await api('POST', `/devices/${id}/regenerate-key`);
    setTimeout(() => showKeyModal(d.name, r.api_key), 50);
    refresh();
  });

  $('#btnDel').onclick = () => openModal(`
    <h2>Xoá thiết bị “${esc(d.name)}”?</h2>
    <p>Toàn bộ lịch sử cảm biến và nhật ký bơm của thiết bị sẽ bị xoá vĩnh viễn.</p>
    <div class="modal-actions"><button class="btn ghost" value="cancel">Huỷ</button><button class="btn danger" value="ok">Xoá vĩnh viễn</button></div>
  `, async () => {
    await api('DELETE', `/devices/${id}`);
    toast('Đã xoá thiết bị');
    location.hash = '#/devices';
  });

  $('#btnCsv').onclick = (e) => run(e.currentTarget, async () => {
    const res = await fetch(`/api/devices/${id}/export.csv?range=${state.range}`, { headers: { Authorization: 'Bearer ' + state.token } });
    if (!res.ok) throw new Error('Không tải được dữ liệu');
    const url = URL.createObjectURL(await res.blob());
    const a = Object.assign(document.createElement('a'), { href: url, download: `${d.name}_${state.range}.csv` });
    a.click();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
  });

  $$('#ranges button').forEach((b) => (b.onclick = () => { state.range = b.dataset.r; loadChart(); }));

  // ---- Biểu đồ ----
  async function loadChart() {
    $$('#ranges button').forEach((b) => b.classList.toggle('active', b.dataset.r === state.range));
    const { points } = await api('GET', `/devices/${id}/history?range=${state.range}`);
    $('#chartEmpty').hidden = points.length > 0;
    const short = ['1h', '6h', '24h'].includes(state.range);
    const labels = points.map((p) => new Date(p.bucket).toLocaleString('vi-VN', short
      ? { hour: '2-digit', minute: '2-digit', hour12: false }
      : { day: '2-digit', month: '2-digit', hour: '2-digit', minute: '2-digit', hour12: false }));
    const css = getComputedStyle(document.documentElement);
    const c = (n) => css.getPropertyValue(n).trim();
    const series = [
      { label: 'Nhiệt độ (°C)', data: points.map((p) => p.temperature), borderColor: '#d9730d', backgroundColor: '#d9730d', yAxisID: 'yT' },
      { label: 'Độ ẩm KK (%)', data: points.map((p) => p.humidity), borderColor: '#8e59c7', backgroundColor: '#8e59c7', yAxisID: 'yP' },
      { label: 'Độ ẩm đất (%)', data: points.map((p) => p.soil_moisture), borderColor: c('--water'), backgroundColor: c('--water'), yAxisID: 'yP', borderWidth: 2.5 },
      { label: 'Bơm chạy (%)', data: points.map((p) => (p.pump_ratio == null ? null : p.pump_ratio * 100)), borderColor: 'transparent', backgroundColor: c('--water-soft'), fill: 'origin', stepped: true, yAxisID: 'yP', pointRadius: 0, order: 10 },
    ];
    if (typeof Chart === 'undefined') { $('#chartEmpty').hidden = false; $('#chartEmpty').textContent = 'Không tải được thư viện biểu đồ'; return; }
    if (state.chart) {
      state.chart.data.labels = labels;
      state.chart.data.datasets.forEach((ds, i) => (ds.data = series[i].data));
      state.chart.update('none');
      return;
    }
    Chart.defaults.color = c('--muted');
    Chart.defaults.font.family = getComputedStyle(document.body).fontFamily;
    state.chart = new Chart($('#chart'), {
      type: 'line',
      data: { labels, datasets: series.map((s) => ({ borderWidth: 2, pointRadius: 0, tension: 0.3, spanGaps: true, ...s })) },
      options: {
        responsive: true, maintainAspectRatio: false, animation: false,
        interaction: { mode: 'index', intersect: false },
        plugins: { legend: { labels: { boxWidth: 10, boxHeight: 10, usePointStyle: true } } },
        scales: {
          x: { grid: { display: false }, ticks: { maxTicksLimit: 8, maxRotation: 0 } },
          yT: { position: 'left', title: { display: true, text: '°C' }, grid: { color: c('--border') } },
          yP: { position: 'right', min: 0, max: 100, title: { display: true, text: '%' }, grid: { display: false } },
        },
      },
    });
  }

  async function loadEvents() {
    const { events } = await api('GET', `/devices/${id}/events?limit=50`);
    const SRC = { auto: 'Tự động', manual: 'Thủ công (web)', local: 'Tại chỗ', safety: 'Ngắt an toàn', boot: 'Khởi động', cloud: 'Web' };
    $('#events').innerHTML = events.length
      ? events.map((e) => `<li><span><span class="badge ${e.pump_on ? 'water' : 'off'}">${e.pump_on ? 'BẬT' : 'TẮT'}</span>
          <span class="muted" style="margin-left:6px">${esc(SRC[e.source] || e.source)}</span></span>
          <span class="muted num">${fmtTime(e.created_at)}</span></li>`).join('')
      : '<li class="muted">Chưa có lần bơm nào</li>';
  }

  paint();
  await Promise.all([loadChart(), loadEvents()]).catch((e) => toast(e.message, true));
  every(3000, () => refresh().catch(() => {}));
  every(30000, () => { loadChart().catch(() => {}); loadEvents().catch(() => {}); });
}

// ---------------- Tài khoản ----------------
function viewAccount() {
  const u = state.user;
  app.innerHTML = `
    <div class="page-head"><div><h1>Tài khoản</h1><p class="muted">${esc(u.email)} · ${u.role === 'admin' ? 'Quản trị viên' : 'Người dùng'}</p></div></div>
    <div class="grid2" style="align-items:start">
      <form class="card" id="fProfile">
        <div class="card-head"><h2>Thông tin cá nhân</h2></div>
        <div class="field"><label for="full_name">Họ và tên</label><input id="full_name" name="full_name" required maxlength="80" value="${esc(u.full_name)}"></div>
        <div class="field"><label for="phone">Số điện thoại</label><input id="phone" name="phone" maxlength="20" value="${esc(u.phone || '')}"></div>
        <button class="btn">Lưu thông tin</button>
      </form>
      <form class="card" id="fPw">
        <div class="card-head"><h2>Đổi mật khẩu</h2></div>
        <div class="field"><label for="cur">Mật khẩu hiện tại</label><input id="cur" name="current_password" type="password" autocomplete="current-password" required></div>
        <div class="field"><label for="np">Mật khẩu mới</label><input id="np" name="new_password" type="password" minlength="6" autocomplete="new-password" required></div>
        <button class="btn">Đổi mật khẩu</button>
      </form>
    </div>`;
  $('#fProfile').onsubmit = (e) => {
    e.preventDefault();
    run(e.submitter, async () => {
      const r = await api('PATCH', '/auth/me', Object.fromEntries(new FormData(e.target)));
      state.user = r.user;
      toast('Đã lưu thông tin');
    });
  };
  $('#fPw').onsubmit = (e) => {
    e.preventDefault();
    run(e.submitter, async () => {
      await api('POST', '/auth/change-password', Object.fromEntries(new FormData(e.target)));
      e.target.reset();
      toast('Đã đổi mật khẩu');
    });
  };
}

// ---------------- Quản trị ----------------
async function viewAdmin() {
  if (state.user.role !== 'admin') { location.hash = '#/devices'; return; }
  app.innerHTML = `
    <div class="page-head"><div><h1>Quản trị hệ thống</h1><p class="muted">Quản lý người dùng và toàn bộ thiết bị</p></div></div>
    <div class="tiles" id="tiles"></div>
    <div class="card">
      <div class="card-head"><h2>Người dùng</h2><button class="btn sm" id="btnNewUser" type="button">+ Tạo tài khoản</button></div>
      <div class="table-wrap"><table>
        <thead><tr><th>Họ tên</th><th>Email</th><th>SĐT</th><th>Vai trò</th><th>Thiết bị</th><th>Đăng nhập gần nhất</th><th>Trạng thái</th><th></th></tr></thead>
        <tbody id="users"></tbody></table></div>
    </div>
    <div class="card">
      <div class="card-head"><h2>Tất cả thiết bị</h2></div>
      <div class="table-wrap"><table>
        <thead><tr><th>Tên</th><th>Chủ sở hữu</th><th>Trạng thái</th><th>Chế độ</th><th>Đất</th><th>Bơm</th><th>Lần cuối</th></tr></thead>
        <tbody id="devs"></tbody></table></div>
    </div>`;

  let users = [];

  async function loadStats() {
    const s = await api('GET', '/admin/stats');
    const tile = (k, v, sub = '') => `<div class="card stat"><div class="k">${k}</div><div class="v num">${v}</div><div class="sub">${sub}</div></div>`;
    $('#tiles').innerHTML = tile('Người dùng', s.users) + tile('Thiết bị', s.devices) +
      tile('Đang trực tuyến', s.online) + tile('Đang bơm', s.pumping) +
      tile('Bản ghi 24 giờ', s.readings24h, `giữ ${s.retention_days} ngày`);
  }

  async function loadUsers() {
    ({ users } = await api('GET', '/admin/users'));
    $('#users').innerHTML = users.map((u) => {
      const me = u.id === state.user.id;
      return `<tr data-id="${u.id}">
        <td>${esc(u.full_name)}${me ? ' <span class="muted small">(bạn)</span>' : ''}</td>
        <td>${esc(u.email)}</td>
        <td>${esc(u.phone || '—')}</td>
        <td><select data-act="role" ${me ? 'disabled' : ''}>
          <option value="user" ${u.role === 'user' ? 'selected' : ''}>Người dùng</option>
          <option value="admin" ${u.role === 'admin' ? 'selected' : ''}>Quản trị viên</option></select></td>
        <td class="num">${u.device_count}</td>
        <td class="small">${fmtTime(u.last_login_at)}</td>
        <td><span class="badge ${u.is_active ? 'on' : 'off'}">${u.is_active ? 'Hoạt động' : 'Đã khoá'}</span></td>
        <td><div class="row">
          ${me ? '' : `<button class="btn ghost sm" data-act="toggle" type="button">${u.is_active ? 'Khoá' : 'Mở khoá'}</button>`}
          <button class="btn ghost sm" data-act="pw" type="button">Đặt lại MK</button>
          ${me ? '' : '<button class="btn danger ghost sm" data-act="del" type="button">Xoá</button>'}
        </div></td></tr>`;
    }).join('');
  }

  async function loadDevs() {
    const { devices } = await api('GET', '/devices?all=1');
    const opts = (sel) => `<option value="">— Chưa có chủ —</option>` +
      users.map((u) => `<option value="${u.id}" ${u.id === sel ? 'selected' : ''}>${esc(u.email)}</option>`).join('');
    $('#devs').innerHTML = devices.length ? devices.map((d) => `<tr data-id="${d.id}">
      <td><a href="#/device/${d.id}">${esc(d.name)}</a><div class="small muted">${esc(d.location)}</div></td>
      <td><select data-act="owner">${opts(d.owner_id)}</select></td>
      <td><span class="badge ${d.online ? 'on' : 'off'}">${d.online ? 'Trực tuyến' : 'Ngoại tuyến'}</span></td>
      <td>${d.mode === 'auto' ? 'Tự động' : 'Thủ công'}</td>
      <td class="num">${fmt(d.soil_moisture, 0)}%</td>
      <td>${d.pump_state ? '<span class="badge water">Chạy</span>' : '<span class="badge off">Tắt</span>'}</td>
      <td class="small">${timeAgo(d.seconds_since_seen)}</td></tr>`).join('')
      : '<tr><td colspan="7" class="muted">Chưa có thiết bị nào</td></tr>';
  }

  $('#users').onchange = (e) => {
    if (e.target.dataset.act !== 'role') return;
    const id = e.target.closest('tr').dataset.id;
    run(e.target, async () => { await api('PATCH', `/admin/users/${id}`, { role: e.target.value }); toast('Đã đổi vai trò'); })
      .finally(() => loadUsers().catch(() => {}));
  };
  $('#users').onclick = (e) => {
    const btn = e.target.closest('button[data-act]');
    if (!btn) return;
    const id = btn.closest('tr').dataset.id;
    const u = users.find((x) => x.id === id);
    if (btn.dataset.act === 'toggle') {
      run(btn, async () => {
        await api('PATCH', `/admin/users/${id}`, { is_active: !u.is_active });
        toast(u.is_active ? 'Đã khoá tài khoản' : 'Đã mở khoá');
        await loadUsers();
      });
    } else if (btn.dataset.act === 'pw') {
      openModal(`<h2>Đặt lại mật khẩu</h2><p class="muted">${esc(u.email)}</p>
        <div class="field"><label for="p">Mật khẩu mới</label><input id="p" name="p" type="text" minlength="6" required autocomplete="off"></div>
        <div class="modal-actions"><button class="btn ghost" value="cancel" formnovalidate>Huỷ</button><button class="btn" value="ok">Đặt lại</button></div>`,
      async (fd) => { await api('POST', `/admin/users/${id}/reset-password`, { new_password: fd.get('p') }); toast('Đã đặt lại mật khẩu'); });
    } else if (btn.dataset.act === 'del') {
      openModal(`<h2>Xoá tài khoản?</h2><p><b>${esc(u.email)}</b> sẽ bị xoá. ${u.device_count ? `${u.device_count} thiết bị của người này sẽ chuyển thành “chưa có chủ”.` : ''}</p>
        <div class="modal-actions"><button class="btn ghost" value="cancel">Huỷ</button><button class="btn danger" value="ok">Xoá</button></div>`,
      async () => { await api('DELETE', `/admin/users/${id}`); toast('Đã xoá tài khoản'); await loadUsers(); await Promise.all([loadDevs(), loadStats()]); });
    }
  };
  $('#devs').onchange = (e) => {
    if (e.target.dataset.act !== 'owner') return;
    const id = e.target.closest('tr').dataset.id;
    run(e.target, async () => {
      await api('PATCH', `/devices/${id}`, { owner_id: e.target.value || null });
      toast('Đã chuyển chủ sở hữu');
      await loadUsers();
    });
  };
  $('#btnNewUser').onclick = () => openModal(`
    <h2>Tạo tài khoản</h2>
    <div class="field"><label for="a">Họ và tên</label><input id="a" name="full_name" required maxlength="80"></div>
    <div class="field"><label for="b">Email</label><input id="b" name="email" type="email" required></div>
    <div class="grid2">
      <div class="field"><label for="c">Mật khẩu</label><input id="c" name="password" type="text" minlength="6" required autocomplete="off"></div>
      <div class="field"><label for="d">Vai trò</label><select id="d" name="role"><option value="user">Người dùng</option><option value="admin">Quản trị viên</option></select></div>
    </div>
    <div class="field"><label for="e">Số điện thoại</label><input id="e" name="phone" maxlength="20"></div>
    <div class="modal-actions"><button class="btn ghost" value="cancel" formnovalidate>Huỷ</button><button class="btn" value="ok">Tạo</button></div>
  `, async (fd) => {
    await api('POST', '/admin/users', Object.fromEntries(fd));
    toast('Đã tạo tài khoản');
    await loadUsers();
    await Promise.all([loadDevs(), loadStats()]);
  });

  await loadStats();
  await loadUsers();
  await loadDevs();
  every(10000, () => { loadStats().catch(() => {}); loadDevs().catch(() => {}); });
}

// ---------------- Điều hướng ----------------
async function route() {
  clearTimers();
  if ($('#modal').open) $('#modal').close();
  const [, page = '', id] = (location.hash || '#/').split('/');

  if (!state.token) {
    $('#topbar').hidden = true;
    return page === 'register' ? viewRegister() : viewLogin();
  }
  if (!state.user) {
    try { ({ user: state.user } = await api('GET', '/auth/me')); }
    catch { return; } // api() đã chuyển về trang đăng nhập
  }
  $('#topbar').hidden = false;
  $('#navAdmin').hidden = state.user.role !== 'admin';
  $$('#topbar nav a').forEach((a) => a.classList.toggle('active', a.dataset.nav === (page === 'device' ? 'devices' : page)));

  try {
    if (page === 'device' && id) await viewDevice(id);
    else if (page === 'account') viewAccount();
    else if (page === 'admin') await viewAdmin();
    else if (page === 'devices') await viewDevices();
    else location.hash = '#/devices';
  } catch (e) {
    app.innerHTML = `<div class="card empty"><h2>Không tải được trang</h2><p class="muted">${esc(e.message)}</p><a class="btn ghost" href="#/devices">Về danh sách thiết bị</a></div>`;
  }
}

$('#btnLogout').onclick = () => logout();
window.addEventListener('hashchange', route);
route();
