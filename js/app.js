import { createStore } from './store.js';
import { PROVINCES, CATEGORIES, categoryOf, TERMS, orderStatusLabel } from './data.js';
import {
  esc, safeImg, fmtMoney, fmtNum, fmtTime, normalizePhone, isValidTazkira, toLatinDigits, compressImage,
} from './util.js';

const $ = (sel, el = document) => el.querySelector(sel);
const view = $('#view');
const tabbar = $('#tabbar');

let store;
let user = null;
let ready = false;
let seq = 0;
let cleanup = null;

// ---------- آیکون‌ها ----------
const svg = (d) => `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">${d}</svg>`;
const I = {
  home: svg('<path d="M3 10.5 12 3l9 7.5V20a1 1 0 0 1-1 1h-5v-6H9v6H4a1 1 0 0 1-1-1z"/>'),
  chat: svg('<path d="M21 12a8 8 0 0 1-11.6 7.1L4 20l1-4.6A8 8 0 1 1 21 12z"/>'),
  plus: svg('<path d="M12 5v14M5 12h14"/>'),
  box: svg('<path d="M21 8 12 3 3 8v8l9 5 9-5z"/><path d="m3 8 9 5 9-5M12 13v8"/>'),
  user: svg('<circle cx="12" cy="8" r="4"/><path d="M4 21a8 8 0 0 1 16 0"/>'),
  back: svg('<path d="m9 6 6 6-6 6"/>'),
  pin: svg('<path d="M12 21s-7-6.2-7-12a7 7 0 0 1 14 0c0 5.8-7 12-7 12z"/><circle cx="12" cy="9" r="2.5"/>'),
  search: svg('<circle cx="11" cy="11" r="7"/><path d="m20 20-3.5-3.5"/>'),
  phone: svg('<path d="M5 4h4l2 5-2.5 1.5a11 11 0 0 0 5 5L15 13l5 2v4a2 2 0 0 1-2 2A16 16 0 0 1 3 6a2 2 0 0 1 2-2z"/>'),
  wallet: svg('<rect x="3" y="6" width="18" height="14" rx="2"/><path d="M3 10h18M16 15h2"/>'),
  shield: svg('<path d="M12 3 4 6v6c0 5 3.5 8 8 9 4.5-1 8-4 8-9V6z"/><path d="m9 12 2 2 4-4"/>'),
  logout: svg('<path d="M15 4h4a1 1 0 0 1 1 1v14a1 1 0 0 1-1 1h-4M10 17l-5-5 5-5M5 12h11"/>'),
  send: svg('<path d="M20 4 3 11l7 2 2 7z"/>'),
  store: svg('<path d="M4 9 5.5 4h13L20 9M4 9v11h16V9M4 9h16M9 20v-6h6v6"/>'),
  doc: svg('<path d="M7 3h7l5 5v13H7z"/><path d="M14 3v5h5M10 13h6M10 17h6"/>'),
  globe: svg('<circle cx="12" cy="12" r="9"/><path d="M3 12h18M12 3a14 14 0 0 1 0 18M12 3a14 14 0 0 0 0 18"/>'),
  truck: svg('<path d="M3 6h11v10H3zM14 9h4l3 3v4h-7"/><circle cx="7" cy="17" r="2"/><circle cx="17" cy="17" r="2"/>'),
};

// ---------- ابزارهای کوچک ----------
function toast(msg, kind = '') {
  const t = $('#toast');
  t.textContent = msg;
  t.className = `toast show ${kind}`;
  clearTimeout(toast.timer);
  toast.timer = setTimeout(() => (t.className = 'toast'), 2600);
}

const go = (path) => {
  if (location.hash === '#' + path) render();
  else location.hash = '#' + path;
};

function header(title, { back = false, right = '' } = {}) {
  return `
    <header class="bar">
      ${back ? `<button class="icon-btn" data-back aria-label="برگشت">${I.back}</button>` : ''}
      <h1 class="bar__title">${title}</h1>
      <div class="bar__right">${right}</div>
    </header>`;
}

function bindCommon() {
  view.querySelectorAll('[data-back]').forEach((b) =>
    b.addEventListener('click', () => (history.length > 1 ? history.back() : go('/home'))));
}

// دکمه را در حین کار غیرفعال می‌کند و خطا را نشان می‌دهد
async function busy(btn, fn) {
  const label = btn.innerHTML;
  btn.disabled = true;
  btn.innerHTML = '<span class="spinner"></span>';
  try {
    return await fn();
  } catch (e) {
    toast(e.message, 'error');
  } finally {
    if (btn.isConnected) {
      btn.disabled = false;
      btn.innerHTML = label;
    }
  }
}

const loading = () => '<div class="loading"><span class="spinner"></span></div>';
const empty = (icon, title, text = '', action = '') =>
  `<div class="empty"><div class="empty__icon">${icon}</div><h3>${title}</h3>${text ? `<p>${text}</p>` : ''}${action}</div>`;

const thumb = (l) =>
  safeImg(l.imageUrl)
    ? `<img src="${esc(safeImg(l.imageUrl))}" alt="" loading="lazy">`
    : `<span class="thumb-icon">${categoryOf(l.category).icon}</span>`;

const sellerLabel = (l) => (l.sellerRole === 'shop' ? `🏪 ${esc(l.shopName || l.sellerName)}` : esc(l.sellerName));

const provinceOptions = (selected) =>
  PROVINCES.map((p) => `<option ${p === selected ? 'selected' : ''}>${p}</option>`).join('');

const formData = (form) => Object.fromEntries(new FormData(form).entries());

// ---------- مسیرها ----------
const routes = [
  ['/welcome', viewWelcome, true],
  ['/login', viewLogin, true],
  ['/register', viewRegister, true],
  ['/terms', viewTerms, true],
  ['/home', viewHome],
  ['/listing/:id', viewListing],
  ['/buy/:id', viewCheckout],
  ['/sell', viewSell],
  ['/orders', viewOrders],
  ['/chats', viewChats],
  ['/chat/:id', viewChat],
  ['/profile', viewProfile],
  ['/wallet', viewWallet],
];

function match(path) {
  for (const [pattern, fn, isPublic] of routes) {
    const keys = [];
    const re = new RegExp('^' + pattern.replace(/:(\w+)/g, (_, k) => (keys.push(k), '([^/]+)')) + '$');
    const m = path.match(re);
    if (m) return { fn, isPublic, params: Object.fromEntries(keys.map((k, i) => [k, decodeURIComponent(m[i + 1])])) };
  }
  return null;
}

async function render() {
  if (!ready) return;
  cleanup?.();
  cleanup = null;
  const token = ++seq;
  const path = location.hash.slice(1) || (user ? '/home' : '/welcome');
  const r = match(path);
  if (!r) return go(user ? '/home' : '/welcome');
  if (!r.isPublic && !user) return go('/welcome');
  if (user && ['/welcome', '/login', '/register'].includes(path)) return go('/home');

  renderTabbar(path);
  window.scrollTo(0, 0);
  // اگر کاربر در حین بارگذاری صفحهٔ دیگری را باز کند، نتیجهٔ قدیمی نقاشی نمی‌شود
  const paint = (html) => {
    if (token !== seq) return false;
    view.innerHTML = html;
    bindCommon();
    return true;
  };
  paint(loading());
  try {
    await r.fn(paint, r.params);
  } catch (e) {
    paint(header('خطا', { back: true }) + empty('⚠️', 'مشکلی پیش آمد', esc(e.message)));
  }
}

function renderTabbar(path) {
  tabbar.hidden = !user;
  if (!user) return;
  const tab = (href, icon, label) =>
    `<a href="#${href}" class="tab ${path.startsWith(href) ? 'active' : ''}">${icon}<span>${label}</span></a>`;
  tabbar.innerHTML = `
    ${tab('/home', I.home, 'بازار')}
    ${tab('/chats', I.chat, 'پیام‌ها')}
    <a href="#/sell" class="tab tab--fab" aria-label="ثبت آگهی">${I.plus}</a>
    ${tab('/orders', I.box, 'سفارش‌ها')}
    ${tab('/profile', I.user, 'حساب')}`;
}

// ---------- خوش آمدید ----------
async function viewWelcome(paint) {
  paint(`
    <section class="welcome">
      <div class="welcome__hero">
        <div class="logo-mark">X</div>
        <h1>خوش آمدید به <span class="brand">JAF-X</span></h1>
        <p>بازار آنلاین افغانستان. بخرید، بفروشید و با مردم ولایت خود وصل شوید.</p>
      </div>
      <ul class="features">
        <li><span>${I.pin}</span><div><b>بازار ولایت خودتان</b><small>خرید و فروش با مردم نزدیک، در همان ولایتی که هستید.</small></div></li>
        <li><span>${I.store}</span><div><b>دکانداران آنلاین بفروشند</b><small>پیسه آنلاین پرداخت می‌شود و سودا به دروازهٔ خانه می‌رسد.</small></div></li>
        <li><span>${I.globe}</span><div><b>از خارج برای خانواده خرید کنید</b><small>در هر کشوری که هستید، برای فامیل‌تان در افغانستان خرید کنید.</small></div></li>
        <li><span>${I.shield}</span><div><b>امن و قابل اعتماد</b><small>ثبت‌نام همه با تذکره. برخورد غیرقانونی پیگرد قانونی دارد.</small></div></li>
      </ul>
      <div class="welcome__actions">
        <a href="#/register" class="btn btn--primary btn--lg">ساختن حساب جدید</a>
        <a href="#/login" class="btn btn--ghost btn--lg">حساب دارم، ورود</a>
        <a href="#/terms" class="link">شرایط و قوانین</a>
      </div>
      ${store.mode === 'local' ? '<p class="demo-note">حالت آزمایشی: معلومات فقط در همین مرورگر ذخیره می‌شود.</p>' : ''}
    </section>`);
}

// ---------- ورود ----------
async function viewLogin(paint) {
  const demo = store.mode === 'local'
    ? '<div class="note">برای آزمایش، حساب دکاندار: <b dir="ltr">0700000001</b> رمز <b dir="ltr">123456</b></div>'
    : '';
  if (!paint(`
    ${header('ورود به حساب', { back: true })}
    <form class="form page" id="f" novalidate>
      ${demo}
      <label>شماره تلفن<input name="phone" type="tel" inputmode="tel" dir="ltr" placeholder="07xx xxx xxx" autocomplete="tel" required></label>
      <label>رمز عبور<input name="password" type="password" dir="ltr" autocomplete="current-password" required></label>
      <button class="btn btn--primary btn--lg" type="submit">ورود</button>
      <p class="center muted">حساب ندارید؟ <a href="#/register" class="link">ثبت‌نام کنید</a></p>
    </form>`)) return;

  $('#f').addEventListener('submit', (e) => {
    e.preventDefault();
    const d = formData(e.target);
    const phone = normalizePhone(d.phone);
    if (!phone) return toast('شمارهٔ تلفن درست نیست.', 'error');
    busy(e.submitter, async () => {
      await store.login(phone, toLatinDigits(d.password));
      toast('خوش آمدید!');
    });
  });
}

// ---------- ثبت‌نام ----------
async function viewRegister(paint) {
  if (!paint(`
    ${header('ثبت‌نام در JAF-X', { back: true })}
    <form class="form page" id="f" novalidate>
      <fieldset>
        <legend>نوع حساب</legend>
        <div class="choice">
          <label class="choice__item"><input type="radio" name="role" value="person" checked><span>${I.user}<b>شخصی</b><small>خرید و فروش شخصی</small></span></label>
          <label class="choice__item"><input type="radio" name="role" value="shop"><span>${I.store}<b>دکاندار</b><small>فروش آنلاین و کیف پول</small></span></label>
        </div>
      </fieldset>

      <fieldset>
        <legend>معلومات شخصی</legend>
        <label>نام کامل<input name="name" autocomplete="name" required></label>
        <label id="shopNameField" hidden>نام دکان<input name="shopName"></label>
        <label>شماره تلفن<input name="phone" type="tel" inputmode="tel" dir="ltr" placeholder="07xx xxx xxx" autocomplete="tel" required></label>
      </fieldset>

      <fieldset>
        <legend>محل زندگی</legend>
        <label class="check"><input type="checkbox" name="abroad"> در خارج از افغانستان زندگی می‌کنم</label>
        <label id="countryField" hidden>کشور فعلی<input name="country" placeholder="مثلاً آلمان"></label>
        <label><span id="provinceLabel">ولایت</span>
          <select name="province" required><option value="">انتخاب ولایت…</option>${provinceOptions()}</select>
        </label>
        <p class="hint" id="provinceHint">فقط آگهی‌های همین ولایت را می‌بینید و فقط در همین ولایت می‌فروشید. بعداً تغییر نمی‌کند.</p>
        <label>آدرس (ناحیه، گذر، کوچه)<textarea name="address" rows="2" required></textarea></label>
      </fieldset>

      <fieldset>
        <legend>تذکره</legend>
        <label>نمبر تذکره<input name="tazkiraNumber" inputmode="numeric" dir="ltr" placeholder="1400-0101-12345" required></label>
        <label class="upload" id="tazkiraBox">
          <input type="file" name="tazkira" accept="image/*" required>
          <span class="upload__empty">${I.doc}<b>عکس تذکره</b><small>عکس واضح از روی تذکره بگیرید</small></span>
        </label>
        <p class="hint">${I.shield} معلومات تذکره محرمانه است و فقط برای تأیید هویت استفاده می‌شود.</p>
      </fieldset>

      <fieldset>
        <legend>رمز عبور</legend>
        <label>رمز (حداقل ۶ حرف)<input name="password" type="password" dir="ltr" autocomplete="new-password" required minlength="6"></label>
        <label>تکرار رمز<input name="password2" type="password" dir="ltr" autocomplete="new-password" required></label>
      </fieldset>

      <label class="check"><input type="checkbox" name="terms" required> <span><a href="#/terms" class="link">شرایط و قوانین</a> را خواندم و قبول دارم.</span></label>
      <button class="btn btn--primary btn--lg" type="submit">ساختن حساب</button>
    </form>`)) return;

  const f = $('#f');
  let tazkira = null;

  const sync = () => {
    const abroad = f.abroad.checked;
    if (abroad && f.role.value === 'shop') f.querySelector('[value=person]').checked = true;
    f.querySelector('[value=shop]').disabled = abroad;
    $('#shopNameField').hidden = f.role.value !== 'shop';
    $('#countryField').hidden = !abroad;
    $('#provinceLabel').textContent = abroad ? 'ولایت تحویل (جایی که خانواده‌تان است)' : 'ولایت';
    $('#provinceHint').textContent = abroad
      ? 'خریدهای شما به همین ولایت تحویل داده می‌شود. کاربران خارج فقط خرید می‌کنند.'
      : 'فقط آگهی‌های همین ولایت را می‌بینید و فقط در همین ولایت می‌فروشید. بعداً تغییر نمی‌کند.';
  };
  f.addEventListener('change', sync);

  f.tazkira.addEventListener('change', async () => {
    const file = f.tazkira.files[0];
    if (!file) return;
    try {
      tazkira = await compressImage(file, 1200, 0.8);
      $('#tazkiraBox').classList.add('has-image');
      $('#tazkiraBox').style.backgroundImage = `url("${tazkira.dataUrl}")`;
    } catch (e) {
      tazkira = null;
      toast(e.message, 'error');
    }
  });

  f.addEventListener('submit', (e) => {
    e.preventDefault();
    const d = formData(f);
    const phone = normalizePhone(d.phone);
    const abroad = f.abroad.checked;
    const err =
      (!d.name.trim() && 'نام کامل را بنویسید.') ||
      (d.role === 'shop' && !d.shopName.trim() && 'نام دکان را بنویسید.') ||
      (!phone && 'شمارهٔ تلفن درست نیست. مثال: 0790123456') ||
      (abroad && !d.country.trim() && 'کشور فعلی را بنویسید.') ||
      (!d.province && 'ولایت را انتخاب کنید.') ||
      (d.address.trim().length < 5 && 'آدرس را کامل بنویسید.') ||
      (!isValidTazkira(d.tazkiraNumber) && 'نمبر تذکره درست نیست.') ||
      (!tazkira && 'عکس تذکره را انتخاب کنید.') ||
      (d.password.length < 6 && 'رمز باید حداقل ۶ حرف باشد.') ||
      (d.password !== d.password2 && 'رمز و تکرار آن یکی نیستند.') ||
      (!f.terms.checked && 'باید شرایط و قوانین را قبول کنید.');
    if (err) return toast(err, 'error');

    busy(e.submitter, async () => {
      await store.register({
        role: d.role, name: d.name.trim(), shopName: d.role === 'shop' ? d.shopName.trim() : '',
        phone, abroad, country: abroad ? d.country.trim() : '', province: d.province,
        address: d.address.trim(), tazkiraNumber: toLatinDigits(d.tazkiraNumber).trim(), tazkira,
        password: toLatinDigits(d.password),
      });
      toast('حساب شما ساخته شد. خوش آمدید!');
    });
  });
}

// ---------- شرایط ----------
async function viewTerms(paint) {
  paint(`
    ${header('شرایط و قوانین', { back: true })}
    <div class="page">
      <ol class="terms">${TERMS.map((t) => `<li>${esc(t)}</li>`).join('')}</ol>
      <div class="note note--warn">${I.shield} هر کسی که برخورد غیرقانونی کند یا دزدی نماید، مطابق قانون با او برخورد می‌شود.</div>
    </div>`);
}

// ---------- بازار ----------
async function viewHome(paint) {
  const listings = await store.listListings();
  let cat = '';
  let q = '';
  const where = user.abroad ? `تحویل در ${esc(user.province)}` : esc(user.province);

  if (!paint(`
    <header class="home-top">
      <div class="home-top__row">
        <div><small class="muted">سلام ${esc(user.name)} 👋</small><div class="brand brand--sm">JAF-X</div></div>
        <span class="chip chip--pin">${I.pin}${where}</span>
      </div>
      <label class="search">${I.search}<input id="q" type="search" placeholder="جستجو در بازار ${esc(user.province)}…"></label>
    </header>
    <div class="cats" id="cats">
      <button class="cat active" data-cat="">همه</button>
      ${CATEGORIES.map((c) => `<button class="cat" data-cat="${c.id}">${c.icon} ${c.name}</button>`).join('')}
    </div>
    <div class="page page--flush" id="list"></div>`)) return;

  const draw = () => {
    const items = listings.filter((l) => (!cat || l.category === cat) && (!q || l.title.includes(q) || (l.description || '').includes(q)));
    $('#list').innerHTML = items.length
      ? `<div class="grid">${items.map((l) => `
          <a class="card" href="#/listing/${esc(l.id)}">
            <div class="card__img">${thumb(l)}${l.sellerRole === 'shop' ? '<span class="badge-online">خرید آنلاین</span>' : ''}</div>
            <div class="card__body">
              <div class="card__price">${fmtMoney(l.price)}</div>
              <div class="card__title">${esc(l.title)}</div>
              <div class="card__meta">${sellerLabel(l)} · ${fmtTime(l.createdAt)}</div>
            </div>
          </a>`).join('')}</div>`
      : empty('🛍️', 'آگهی پیدا نشد', `هنوز در ${esc(user.province)} آگهی‌ای با این مشخصات نیست.`,
        user.abroad ? '' : '<a href="#/sell" class="btn btn--primary">اولین آگهی را بگذارید</a>');
  };
  draw();

  $('#q').addEventListener('input', (e) => {
    q = e.target.value.trim();
    draw();
  });
  $('#cats').addEventListener('click', (e) => {
    const b = e.target.closest('[data-cat]');
    if (!b) return;
    cat = b.dataset.cat;
    view.querySelectorAll('.cat').forEach((x) => x.classList.toggle('active', x === b));
    draw();
  });
}

// ---------- جزئیات آگهی ----------
async function viewListing(paint, { id }) {
  let l;
  try {
    l = await store.getListing(id);
  } catch (e) {
    return paint(header('آگهی', { back: true }) + empty('🔒', 'دسترسی ندارید', esc(e.message)));
  }
  if (!l) return paint(header('آگهی', { back: true }) + empty('🔍', 'آگهی پیدا نشد'));

  const mine = l.sellerId === user.id;
  const c = categoryOf(l.category);
  const digits = l.sellerPhone.replace('+', '');
  const actions = mine
    ? `<button class="btn btn--ghost btn--lg" id="toggle">${l.status === 'active' ? 'علامت «فروخته شد»' : 'دوباره فعال کن'}</button>`
    : `
      ${l.sellerRole === 'shop' && l.status === 'active' ? `<a href="#/buy/${esc(l.id)}" class="btn btn--primary btn--lg">${I.truck} خرید آنلاین و تحویل در خانه</a>` : ''}
      <div class="row">
        <button class="btn btn--ghost" id="chat">${I.chat} پیام</button>
        <a class="btn btn--ghost" href="tel:${esc(l.sellerPhone)}">${I.phone} تماس</a>
        <a class="btn btn--ghost" href="https://wa.me/${esc(digits)}" target="_blank" rel="noopener">واتساپ</a>
      </div>`;

  if (!paint(`
    ${header('', { back: true })}
    <div class="detail">
      <div class="detail__img">${thumb(l)}</div>
      <div class="page">
        ${l.status !== 'active' ? '<div class="note note--warn">این جنس فروخته شده است.</div>' : ''}
        <div class="detail__price">${fmtMoney(l.price)}</div>
        <h2 class="detail__title">${esc(l.title)}</h2>
        <div class="detail__meta"><span class="chip">${c.icon} ${c.name}</span><span class="chip chip--pin">${I.pin}${esc(l.province)}</span><span class="muted">${fmtTime(l.createdAt)}</span></div>
        ${l.description ? `<p class="detail__desc">${esc(l.description)}</p>` : ''}
        <div class="seller">
          <div class="avatar">${esc((l.shopName || l.sellerName).slice(0, 1))}</div>
          <div><b>${esc(l.shopName || l.sellerName)}</b><small class="muted">${l.sellerRole === 'shop' ? 'دکاندار' : 'فروشندهٔ شخصی'} · ${esc(l.province)}</small></div>
        </div>
        <div class="stack">${actions}</div>
        <p class="hint">${I.shield} قبل از پرداخت، جنس را ببینید. در صورت تقلب به JAF-X گزارش دهید.</p>
      </div>
    </div>`)) return;

  $('#chat')?.addEventListener('click', (e) =>
    busy(e.currentTarget, async () => go('/chat/' + (await store.openChat(l)))));
  $('#toggle')?.addEventListener('click', (e) =>
    busy(e.currentTarget, async () => {
      await store.setListingStatus(l.id, l.status === 'active' ? 'sold' : 'active');
      render();
    }));
}

// ---------- خرید آنلاین ----------
async function viewCheckout(paint, { id }) {
  const l = await store.getListing(id);
  if (!l) return paint(header('خرید', { back: true }) + empty('🔍', 'آگهی پیدا نشد'));
  const online = store.onlinePayment;
  const self = !user.abroad;

  if (!paint(`
    ${header('تکمیل خرید', { back: true })}
    <form class="form page" id="f" novalidate>
      <div class="summary">
        <div class="summary__img">${thumb(l)}</div>
        <div><b>${esc(l.title)}</b><small class="muted">${sellerLabel(l)}</small><div class="card__price">${fmtMoney(l.price)}</div></div>
      </div>

      <fieldset>
        <legend>${I.truck} آدرس تحویل در ${esc(l.province)}</legend>
        ${user.abroad ? '<p class="hint">معلومات عضو خانواده‌تان را بنویسید که سودا را تحویل می‌گیرد.</p>' : ''}
        <label>نام تحویل‌گیرنده<input name="name" value="${self ? esc(user.name) : ''}" required></label>
        <label>شماره تلفن تحویل‌گیرنده<input name="phone" type="tel" dir="ltr" value="${self ? esc(user.phone) : ''}" required></label>
        <label>آدرس کامل<textarea name="address" rows="2" required>${self ? esc(user.address) : ''}</textarea></label>
      </fieldset>

      <fieldset>
        <legend>روش پرداخت</legend>
        <div class="choice choice--col">
          <label class="choice__item"><input type="radio" name="pay" value="online" ${online ? 'checked' : 'disabled'}>
            <span>${I.wallet}<b>پرداخت آنلاین${store.mode === 'local' ? ' (آزمایشی)' : ''}</b><small>${online ? 'پیسه حالا پرداخت می‌شود و دکاندار سودا را به خانه می‌آورد.' : 'به‌زودی (HesabPay / M-Paisa)'}</small></span></label>
          <label class="choice__item"><input type="radio" name="pay" value="cod" ${online ? '' : 'checked'}>
            <span>${I.truck}<b>پرداخت هنگام تحویل</b><small>پیسه را هنگام رسیدن سودا نقد بپردازید.</small></span></label>
        </div>
      </fieldset>

      <div class="total"><span>مبلغ قابل پرداخت</span><b>${fmtMoney(l.price)}</b></div>
      <button class="btn btn--primary btn--lg" type="submit">ثبت سفارش</button>
    </form>`)) return;

  $('#f').addEventListener('submit', (e) => {
    e.preventDefault();
    const d = formData(e.target);
    busy(e.submitter, async () => {
      const delivery = { name: d.name.trim(), phone: normalizePhone(d.phone), address: d.address.trim() };
      const orderId = await store.createOrder(l.id, delivery, d.pay);
      if (d.pay === 'online') {
        await store.payOrder(orderId);
        toast('پرداخت انجام شد. دکاندار سودا را به آدرس شما می‌فرستد.');
      } else {
        toast('سفارش ثبت شد. پیسه را هنگام تحویل بپردازید.');
      }
      go('/orders');
    });
  });
}

// ---------- ثبت آگهی ----------
async function viewSell(paint) {
  if (user.abroad) {
    return paint(header('ثبت آگهی') + empty('✈️', 'فروش برای کاربران خارج ممکن نیست',
      `شما می‌توانید از بازار ${esc(user.province)} برای خانواده‌تان خرید کنید.`,
      '<a href="#/home" class="btn btn--primary">رفتن به بازار</a>'));
  }

  if (!paint(`
    ${header('ثبت آگهی جدید')}
    <form class="form page" id="f" novalidate>
      <label class="upload upload--big" id="imgBox">
        <input type="file" name="image" accept="image/*">
        <span class="upload__empty">📷<b>افزودن عکس</b><small>آگهی عکس‌دار زودتر فروخته می‌شود</small></span>
      </label>
      <label>عنوان<input name="title" maxlength="80" placeholder="مثلاً: گوشی سامسونگ A54" required></label>
      <label>قیمت (افغانی)<input name="price" inputmode="numeric" dir="ltr" placeholder="0" required></label>
      <fieldset>
        <legend>دسته‌بندی</legend>
        <div class="cat-pick">${CATEGORIES.map((c, i) => `
          <label><input type="radio" name="category" value="${c.id}" ${i === 0 ? 'checked' : ''}><span>${c.icon}<small>${c.name}</small></span></label>`).join('')}
        </div>
      </fieldset>
      <label>توضیحات<textarea name="description" rows="4" maxlength="1000" placeholder="حالت جنس، رنگ، اندازه…"></textarea></label>
      <div class="note">${I.pin} این آگهی فقط برای مردم ولایت <b>${esc(user.province)}</b> نشان داده می‌شود.
        ${user.role === 'shop' ? '<br>چون دکاندار هستید، مشتریان می‌توانند آنلاین بخرند.' : ''}</div>
      <button class="btn btn--primary btn--lg" type="submit">انتشار آگهی</button>
    </form>`)) return;

  const f = $('#f');
  let image = null;
  f.image.addEventListener('change', async () => {
    const file = f.image.files[0];
    if (!file) return;
    try {
      image = await compressImage(file);
      $('#imgBox').classList.add('has-image');
      $('#imgBox').style.backgroundImage = `url("${image.dataUrl}")`;
    } catch (e) {
      image = null;
      toast(e.message, 'error');
    }
  });

  f.addEventListener('submit', (e) => {
    e.preventDefault();
    const d = formData(f);
    busy(e.submitter, async () => {
      const id = await store.createListing({
        title: d.title.trim(),
        price: Number(toLatinDigits(d.price).replace(/[^\d]/g, '')),
        category: d.category,
        description: d.description.trim(),
      }, image);
      toast('آگهی منتشر شد.');
      go('/listing/' + id);
    });
  });
}

// ---------- سفارش‌ها ----------
async function viewOrders(paint) {
  const { buying, selling } = await store.myOrders();
  let tab = user.role === 'shop' && selling.length ? 'selling' : 'buying';

  if (!paint(`
    ${header('سفارش‌ها')}
    <div class="tabs" id="tabs">
      <button data-tab="buying">خریدهای من (${fmtNum(buying.length)})</button>
      <button data-tab="selling">فروش‌های من (${fmtNum(selling.length)})</button>
    </div>
    <div class="page" id="list"></div>`)) return;

  const card = (o, side) => {
    let act = '';
    if (side === 'selling') {
      if (o.status === 'paid' || (o.payMethod === 'cod' && o.status === 'pending')) act = `<button class="btn btn--primary btn--sm" data-act="shipped" data-id="${esc(o.id)}">${I.truck} ارسال شد</button>`;
      else if (o.status === 'shipped') act = `<button class="btn btn--primary btn--sm" data-act="delivered" data-id="${esc(o.id)}">تحویل داده شد ✓</button>`;
    } else if (o.payMethod === 'online' && o.status === 'pending' && store.onlinePayment) {
      act = `<button class="btn btn--primary btn--sm" data-act="pay" data-id="${esc(o.id)}">پرداخت</button>`;
    }
    return `
      <article class="order">
        <div class="order__top">
          <div class="summary__img">${thumb(o)}</div>
          <div class="grow"><b>${esc(o.title)}</b><small class="muted">${side === 'buying' ? esc(o.shopName) : 'خریدار: ' + esc(o.buyerName)} · ${fmtTime(o.createdAt)}</small></div>
          <span class="status status--${esc(o.status)}">${orderStatusLabel(o)}</span>
        </div>
        <div class="order__info">
          <span>${fmtMoney(o.price)} · ${o.payMethod === 'cod' ? 'پرداخت هنگام تحویل' : o.paid ? 'آنلاین پرداخت شد' : 'آنلاین'}</span>
          ${side === 'selling' ? `<span>${I.pin} ${esc(o.delivery.name)} · <a dir="ltr" href="tel:${esc(o.delivery.phone)}">${esc(o.delivery.phone)}</a><br>${esc(o.delivery.address)}، ${esc(o.province)}</span>` : ''}
        </div>
        ${act ? `<div class="order__act">${act}</div>` : ''}
      </article>`;
  };

  const draw = () => {
    view.querySelectorAll('[data-tab]').forEach((b) => b.classList.toggle('active', b.dataset.tab === tab));
    const items = tab === 'buying' ? buying : selling;
    $('#list').innerHTML = items.length
      ? items.map((o) => card(o, tab)).join('')
      : empty('📦', 'هنوز سفارشی نیست', tab === 'buying' ? 'از دکانداران ولایت خود آنلاین خرید کنید.' : 'سفارش‌های مشتریان شما اینجا نشان داده می‌شود.');
  };
  draw();

  $('#tabs').addEventListener('click', (e) => {
    const b = e.target.closest('[data-tab]');
    if (b) {
      tab = b.dataset.tab;
      draw();
    }
  });
  $('#list').addEventListener('click', (e) => {
    const b = e.target.closest('[data-act]');
    if (!b) return;
    busy(b, async () => {
      if (b.dataset.act === 'pay') await store.payOrder(b.dataset.id);
      else await store.updateOrderStatus(b.dataset.id, b.dataset.act);
      toast('ثبت شد.');
      render();
    });
  });
}

// ---------- پیام‌ها ----------
async function viewChats(paint) {
  const chats = await store.myChats();
  const other = (c) => c.names?.[c.members.find((m) => m !== user.id)] || 'کاربر';
  paint(`
    ${header('پیام‌ها')}
    <div class="page page--flush">
      ${chats.length
        ? `<ul class="chat-list">${chats.map((c) => `
            <li><a href="#/chat/${esc(c.id)}">
              <div class="avatar">${esc(other(c).slice(0, 1))}</div>
              <div class="grow"><b>${esc(other(c))}</b><small class="muted">${esc(c.listingTitle)}</small><p>${esc(c.lastMessage || 'گفتگو شروع شد')}</p></div>
              <small class="muted">${fmtTime(c.updatedAt)}</small>
            </a></li>`).join('')}</ul>`
        : empty('💬', 'هنوز پیامی نیست', `در صفحهٔ هر آگهی دکمهٔ «پیام» را بزنید تا با مردم ${esc(user.province)} گفتگو کنید.`)}
    </div>`);
}

async function viewChat(paint, { id }) {
  const c = await store.getChat(id);
  if (!c) return paint(header('گفتگو', { back: true }) + empty('🔍', 'گفتگو پیدا نشد'));
  const otherName = c.names?.[c.members.find((m) => m !== user.id)] || 'کاربر';

  if (!paint(`
    ${header(`<span>${esc(otherName)}</span><small><a href="#/listing/${esc(c.listingId)}">${esc(c.listingTitle)}</a></small>`, { back: true })}
    <div class="messages" id="msgs"></div>
    <form class="composer" id="f">
      <input name="text" placeholder="پیام بنویسید…" autocomplete="off" maxlength="1000">
      <button class="icon-btn icon-btn--primary" aria-label="فرستادن">${I.send}</button>
    </form>`)) return;

  const box = $('#msgs');
  cleanup = store.watchMessages(id, (msgs) => {
    box.innerHTML = msgs.length
      ? msgs.map((m) => `<div class="msg ${m.senderId === user.id ? 'msg--me' : ''}"><p>${esc(m.text)}</p><small>${fmtTime(m.createdAt)}</small></div>`).join('')
      : '<p class="center muted">سلام کنید و گفتگو را شروع کنید 👋</p>';
    box.scrollTop = box.scrollHeight;
    window.scrollTo(0, document.body.scrollHeight);
  });

  $('#f').addEventListener('submit', async (e) => {
    e.preventDefault();
    const input = e.target.text;
    const text = input.value.trim();
    if (!text) return;
    input.value = '';
    try {
      await store.sendMessage(id, text);
    } catch (err) {
      input.value = text;
      toast(err.message, 'error');
    }
  });
}

// ---------- حساب ----------
async function viewProfile(paint) {
  const mine = await store.myListings();
  if (!paint(`
    ${header('حساب من')}
    <div class="page">
      <div class="profile">
        <div class="avatar avatar--lg">${esc(user.name.slice(0, 1))}</div>
        <div>
          <h2>${esc(user.name)}</h2>
          ${user.shopName ? `<div>🏪 ${esc(user.shopName)}</div>` : ''}
          <small class="muted" dir="ltr">${esc(user.phone)}</small>
        </div>
      </div>
      <div class="info-grid">
        <div><small>نوع حساب</small><b>${user.role === 'shop' ? 'دکاندار' : 'شخصی'}</b></div>
        <div><small>${user.abroad ? 'ولایت تحویل' : 'ولایت'}</small><b>${esc(user.province)}</b></div>
        ${user.abroad ? `<div><small>کشور</small><b>${esc(user.country)}</b></div>` : ''}
        <div><small>تذکره</small><b class="${user.verified ? 'ok' : 'warn'}">${user.verified ? 'تأیید شد ✓' : 'در انتظار تأیید'}</b></div>
      </div>

      <nav class="menu">
        ${user.role === 'shop' ? `<a href="#/wallet">${I.wallet}<span>کیف پول و برداشت پول</span></a>` : ''}
        <a href="#/orders">${I.box}<span>سفارش‌ها</span></a>
        <a href="#/terms">${I.doc}<span>شرایط و قوانین</span></a>
        <button id="logout">${I.logout}<span>خروج از حساب</span></button>
      </nav>

      ${user.abroad ? '' : `
        <h3 class="section-title">آگهی‌های من (${fmtNum(mine.length)})</h3>
        ${mine.length ? `<div class="mini-list">${mine.map((l) => `
          <a href="#/listing/${esc(l.id)}" class="mini">
            <div class="summary__img">${thumb(l)}</div>
            <div class="grow"><b>${esc(l.title)}</b><small class="muted">${fmtMoney(l.price)}</small></div>
            <span class="status ${l.status === 'active' ? 'status--paid' : ''}">${l.status === 'active' ? 'فعال' : 'فروخته شد'}</span>
          </a>`).join('')}</div>` : '<p class="muted">هنوز آگهی نگذاشته‌اید.</p>'}`}
    </div>`)) return;

  $('#logout').addEventListener('click', async () => {
    await store.logout();
    toast('از حساب خارج شدید.');
  });
}

// ---------- کیف پول ----------
async function viewWallet(paint) {
  if (user.role !== 'shop') return go('/profile');
  const w = await store.wallet();
  if (!paint(`
    ${header('کیف پول', { back: true })}
    <div class="page">
      <div class="wallet">
        <small>موجودی قابل برداشت</small>
        <div class="wallet__balance">${fmtMoney(w.balance)}</div>
        <div class="wallet__row"><span>کل فروش آنلاین: ${fmtMoney(w.earned)}</span><span>در راه: ${fmtMoney(w.onTheWay)}</span></div>
      </div>
      <p class="hint">پول هر سفارش آنلاین پس از «تحویل داده شد» به موجودی شما اضافه می‌شود.</p>

      <form class="form card-box" id="f" novalidate>
        <h3>برداشت پول از نمایندگی</h3>
        <label>مبلغ (افغانی)<input name="amount" inputmode="numeric" dir="ltr" required></label>
        <button class="btn btn--primary btn--lg" type="submit" ${w.balance > 0 ? '' : 'disabled'}>گرفتن کد برداشت</button>
        <p class="hint">کد را با تذکرهٔ خود به نزدیک‌ترین نمایندگی JAF-X ببرید و پول را نقد بگیرید.</p>
      </form>

      <h3 class="section-title">برداشت‌ها</h3>
      ${w.withdrawals.length ? w.withdrawals.map((x) => `
        <div class="mini">
          <div class="grow"><b>${fmtMoney(x.amount)}</b><small class="muted">${fmtTime(x.createdAt)}</small></div>
          <span class="code" dir="ltr">${esc(x.code)}</span>
          <span class="status ${x.status === 'done' ? 'status--delivered' : 'status--pending'}">${x.status === 'done' ? 'گرفته شد' : 'در انتظار'}</span>
        </div>`).join('') : '<p class="muted">هنوز برداشتی نکرده‌اید.</p>'}
    </div>`)) return;

  $('#f').addEventListener('submit', (e) => {
    e.preventDefault();
    const amount = Number(toLatinDigits(e.target.amount.value).replace(/[^\d]/g, ''));
    busy(e.submitter, async () => {
      const code = await store.requestWithdrawal(amount);
      toast(`کد برداشت شما: ${code}`);
      render();
    });
  });
}

// ---------- شروع ----------
addEventListener('hashchange', render);

(async () => {
  try {
    store = await createStore();
  } catch (e) {
    view.innerHTML = empty('⚠️', 'اپ بارگذاری نشد', esc(e.message));
    return;
  }
  let first = true;
  store.onAuth((u) => {
    const changed = !first && !!u !== !!user;
    user = u;
    ready = true;
    first = false;
    if (changed) go(u ? '/home' : '/welcome');
    else render();
  });
})();
