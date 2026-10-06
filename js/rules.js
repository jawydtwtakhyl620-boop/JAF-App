// قوانین مشترک اپ — هر دو حالت (آزمایشی و Firebase) از همین‌ها استفاده می‌کنند.
// در Firebase همین قوانین در firestore.rules هم روی سرور اجرا می‌شوند.

export function assertCanSell(user) {
  if (user.abroad) throw new Error('کاربرانی که در خارج هستند فقط می‌توانند خرید کنند.');
}

export function assertSameProvince(user, item) {
  if (item.province !== user.province) {
    throw new Error(`این آگهی مربوط به ولایت ${item.province} است و از ولایت شما قابل دسترس نیست.`);
  }
}

export function assertCanBuyOnline(user, listing) {
  assertSameProvince(user, listing);
  if (listing.sellerId === user.id) throw new Error('نمی‌توانید جنس خودتان را بخرید.');
  if (listing.sellerRole !== 'shop') throw new Error('خرید آنلاین فقط از دکانداران ممکن است.');
  if (listing.status !== 'active') throw new Error('این جنس دیگر موجود نیست.');
}

export function validateListing(d) {
  if (!d.title || d.title.trim().length < 3) throw new Error('عنوان آگهی را بنویسید (حداقل ۳ حرف).');
  if (!(d.price > 0)) throw new Error('قیمت درست وارد کنید.');
  if (!d.category) throw new Error('دسته‌بندی را انتخاب کنید.');
}

export function validateDelivery(d) {
  if (!d.name?.trim()) throw new Error('نام تحویل‌گیرنده را بنویسید.');
  if (!d.phone) throw new Error('شمارهٔ تلفن تحویل‌گیرنده درست نیست.');
  if (!d.address?.trim() || d.address.trim().length < 5) throw new Error('آدرس کامل تحویل را بنویسید.');
}

// موجودی کیف پول: پول فروش‌های آنلاینِ پرداخت‌شده و تحویل‌شده، منهای برداشت‌ها
export function computeWallet(sales, withdrawals) {
  const earned = sales.filter((o) => o.paid && o.status === 'delivered').reduce((s, o) => s + o.price, 0);
  const onTheWay = sales.filter((o) => o.paid && o.status !== 'delivered').reduce((s, o) => s + o.price, 0);
  const withdrawn = withdrawals.filter((w) => w.status !== 'rejected').reduce((s, w) => s + w.amount, 0);
  return { earned, onTheWay, withdrawn, balance: earned - withdrawn };
}

export function chatIdFor(a, b, listingId) {
  return [a, b].sort().join('_') + '_' + listingId;
}
