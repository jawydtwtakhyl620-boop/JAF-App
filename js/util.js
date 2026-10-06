const ESC = { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' };
export const esc = (v) => String(v ?? '').replace(/[&<>"']/g, (c) => ESC[c]);

// فقط عکس‌های data: و https: پذیرفته می‌شوند
export const safeImg = (u) => (typeof u === 'string' && /^(data:image\/|https:\/\/)/.test(u) ? u : '');

export const toLatinDigits = (s) =>
  String(s ?? '')
    .replace(/[۰-۹]/g, (d) => d.charCodeAt(0) - 0x06f0)
    .replace(/[٠-٩]/g, (d) => d.charCodeAt(0) - 0x0660);

const nf = new Intl.NumberFormat('fa-AF');
export const fmtNum = (n) => nf.format(Number(n) || 0);
export const fmtMoney = (n) => `${fmtNum(n)} افغانی`;

export function fmtTime(ms) {
  const diff = (Date.now() - ms) / 1000;
  if (diff < 60) return 'همین حالا';
  if (diff < 3600) return `${fmtNum(Math.floor(diff / 60))} دقیقه پیش`;
  if (diff < 86400) return `${fmtNum(Math.floor(diff / 3600))} ساعت پیش`;
  if (diff < 86400 * 7) return `${fmtNum(Math.floor(diff / 86400))} روز پیش`;
  return new Date(ms).toLocaleDateString('fa-AF');
}

// شمارهٔ افغانستان (07xxxxxxxx) به شکل +937xxxxxxxx؛ شمارهٔ خارجی باید با + یا 00 شروع شود.
export function normalizePhone(raw) {
  let p = toLatinDigits(raw).replace(/[\s\-()]/g, '');
  if (p.startsWith('00')) p = '+' + p.slice(2);
  if (/^07\d{8}$/.test(p)) return '+93' + p.slice(1);
  if (/^7\d{8}$/.test(p)) return '+93' + p;
  if (/^\+?937\d{8}$/.test(p)) return '+' + p.replace(/^\+/, '');
  if (/^\+\d{8,15}$/.test(p)) return p;
  return null;
}

export const isValidTazkira = (t) => /^[0-9\-]{6,20}$/.test(toLatinDigits(t).replace(/\s/g, ''));

export const newId = () => Date.now().toString(36) + Math.random().toString(36).slice(2, 10);

export const randomCode = () => String(Math.floor(100000 + Math.random() * 900000));

export async function sha256(text) {
  if (crypto?.subtle) {
    const buf = await crypto.subtle.digest('SHA-256', new TextEncoder().encode(text));
    return [...new Uint8Array(buf)].map((b) => b.toString(16).padStart(2, '0')).join('');
  }
  let h = 0;
  for (const c of text) h = (Math.imul(31, h) + c.charCodeAt(0)) | 0;
  return 'x' + (h >>> 0).toString(16);
}

// عکس را کوچک و فشرده می‌کند تا آپلود روی اینترنت ضعیف سریع باشد
export async function compressImage(file, max = 1000, quality = 0.75) {
  if (!file || !file.type.startsWith('image/')) throw new Error('لطفاً یک عکس انتخاب کنید.');
  const url = URL.createObjectURL(file);
  try {
    const img = await new Promise((resolve, reject) => {
      const i = new Image();
      i.onload = () => resolve(i);
      i.onerror = () => reject(new Error('عکس خوانده نشد.'));
      i.src = url;
    });
    const scale = Math.min(1, max / Math.max(img.width, img.height));
    const c = document.createElement('canvas');
    c.width = Math.round(img.width * scale);
    c.height = Math.round(img.height * scale);
    c.getContext('2d').drawImage(img, 0, 0, c.width, c.height);
    const dataUrl = c.toDataURL('image/jpeg', quality);
    const blob = await new Promise((r) => c.toBlob(r, 'image/jpeg', quality));
    return { blob, dataUrl };
  } finally {
    URL.revokeObjectURL(url);
  }
}
