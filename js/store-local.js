// حالت آزمایشی: همه‌چیز در localStorage همین مرورگر ذخیره می‌شود.
import { newId, sha256, randomCode } from './util.js';
import * as R from './rules.js';

const KEY = 'jafx-db-v1';
const SESSION = 'jafx-session';

export const DEMO_SHOP = { phone: '+93700000001', password: '123456' };

function read(key) {
  try {
    return JSON.parse(localStorage.getItem(key));
  } catch {
    return null;
  }
}

async function seed() {
  const shopId = 'demo-shop';
  const userId = 'demo-user';
  const now = Date.now();
  const listing = (id, title, price, category, description, ago, seller = shopId) => ({
    id, title, price, category, description, imageUrl: '',
    sellerId: seller,
    sellerName: seller === shopId ? 'احمد' : 'محمود',
    sellerRole: seller === shopId ? 'shop' : 'person',
    shopName: seller === shopId ? 'دکان برادران احمدی' : '',
    sellerPhone: seller === shopId ? DEMO_SHOP.phone : '+93700000002',
    province: 'کابل', status: 'active', createdAt: now - ago * 60000,
  });
  return {
    auth: {
      [DEMO_SHOP.phone]: { uid: shopId, hash: await sha256(shopId + DEMO_SHOP.password) },
      '+93700000002': { uid: userId, hash: await sha256(userId + DEMO_SHOP.password) },
    },
    users: {
      [shopId]: {
        id: shopId, name: 'احمد', shopName: 'دکان برادران احمدی', role: 'shop', phone: DEMO_SHOP.phone,
        province: 'کابل', abroad: false, country: '', verified: true, createdAt: now,
      },
      [userId]: {
        id: userId, name: 'محمود', shopName: '', role: 'person', phone: '+93700000002',
        province: 'کابل', abroad: false, country: '', verified: true, createdAt: now,
      },
    },
    kyc: {},
    listings: Object.fromEntries([
      listing('l1', 'گوشی سامسونگ A54 نو', 18500, 'mobile', 'کارتن‌دار، با گارانتی یک ساله.', 12),
      listing('l2', 'برنج سیلا ۲۵ کیلویی', 2900, 'food', 'برنج اعلای سیلا، تحویل در خانه.', 45),
      listing('l3', 'کمپل زمستانی دو نفره', 1800, 'home', 'کمپل نرم و گرم، رنگ‌های مختلف.', 180),
      listing('l4', 'موتر کرولا ۲۰۰۸', 520000, 'vehicle', 'رنگ سفید، اسناد مکمل، تیل کم مصرف.', 300, userId),
      listing('l5', 'چپن دست‌دوز', 3500, 'clothes', 'چپن اصل، دوخت دستی.', 900),
    ].map((l) => [l.id, l])),
    orders: {},
    chats: {},
    messages: {},
    withdrawals: {},
  };
}

export async function createLocalStore() {
  let db = read(KEY) || (await seed());
  const subs = new Set();
  let authCb = () => {};

  const emit = () => subs.forEach((f) => f());
  const save = () => {
    try {
      localStorage.setItem(KEY, JSON.stringify(db));
    } catch {
      throw new Error('حافظهٔ مرورگر پر است. چند آگهی عکس‌دار را حذف کنید.');
    }
    emit();
  };
  save();

  addEventListener('storage', (e) => {
    if (e.key === KEY) {
      db = read(KEY) || db;
      emit();
    }
  });

  const sessionUid = () => localStorage.getItem(SESSION);
  const profile = () => {
    const u = db.users[sessionUid()];
    return u ? { ...u, address: db.kyc[u.id]?.address || '' } : null;
  };
  const me = () => {
    const u = profile();
    if (!u) throw new Error('لطفاً اول وارد حساب شوید.');
    return u;
  };
  const list = (obj) => Object.values(obj).sort((a, b) => b.createdAt - a.createdAt);

  return {
    mode: 'local',
    onlinePayment: true, // در حالت آزمایشی پرداخت شبیه‌سازی می‌شود

    onAuth(cb) {
      authCb = cb;
      cb(profile());
    },
    me: profile,

    async register(f) {
      if (db.auth[f.phone]) throw new Error('این شماره قبلاً ثبت شده است. وارد حساب شوید.');
      const id = newId();
      db.auth[f.phone] = { uid: id, hash: await sha256(id + f.password) };
      db.users[id] = {
        id, name: f.name, shopName: f.shopName, role: f.role, phone: f.phone,
        province: f.province, abroad: f.abroad, country: f.country, verified: false, createdAt: Date.now(),
      };
      db.kyc[id] = { tazkiraNumber: f.tazkiraNumber, tazkiraPhoto: f.tazkira.dataUrl, address: f.address };
      save();
      localStorage.setItem(SESSION, id);
      authCb(profile());
    },

    async login(phone, password) {
      const rec = db.auth[phone];
      if (!rec || rec.hash !== (await sha256(rec.uid + password))) throw new Error('شماره یا رمز اشتباه است.');
      localStorage.setItem(SESSION, rec.uid);
      authCb(profile());
    },

    async logout() {
      localStorage.removeItem(SESSION);
      authCb(null);
    },

    async listListings() {
      const u = me();
      return list(db.listings).filter((l) => l.province === u.province && l.status === 'active');
    },

    async getListing(id) {
      const l = db.listings[id];
      if (!l) return null;
      R.assertSameProvince(me(), l);
      return l;
    },

    async myListings() {
      const u = me();
      return list(db.listings).filter((l) => l.sellerId === u.id);
    },

    async createListing(d, image) {
      const u = me();
      R.assertCanSell(u);
      R.validateListing(d);
      const id = newId();
      db.listings[id] = {
        id, ...d, imageUrl: image?.dataUrl || '',
        sellerId: u.id, sellerName: u.name, sellerRole: u.role, shopName: u.shopName, sellerPhone: u.phone,
        province: u.province, status: 'active', createdAt: Date.now(),
      };
      save();
      return id;
    },

    async setListingStatus(id, status) {
      const l = db.listings[id];
      if (!l || l.sellerId !== me().id) throw new Error('اجازه ندارید.');
      l.status = status;
      save();
    },

    async createOrder(listingId, delivery, payMethod) {
      const u = me();
      const l = db.listings[listingId];
      if (!l) throw new Error('آگهی پیدا نشد.');
      R.assertCanBuyOnline(u, l);
      R.validateDelivery(delivery);
      const id = newId();
      db.orders[id] = {
        id, listingId, title: l.title, price: l.price, imageUrl: l.imageUrl, category: l.category,
        buyerId: u.id, buyerName: u.name, sellerId: l.sellerId, shopName: l.shopName || l.sellerName,
        province: l.province, delivery, payMethod, paid: false, status: 'pending', createdAt: Date.now(),
      };
      save();
      return id;
    },

    // شبیه‌سازی درگاه پرداخت — در نسخهٔ واقعی سرورِ درگاه این کار را می‌کند
    async payOrder(id) {
      const o = db.orders[id];
      if (!o || o.buyerId !== me().id) throw new Error('سفارش پیدا نشد.');
      o.paid = true;
      o.status = 'paid';
      save();
    },

    async myOrders() {
      const u = me();
      const all = list(db.orders);
      return { buying: all.filter((o) => o.buyerId === u.id), selling: all.filter((o) => o.sellerId === u.id) };
    },

    async updateOrderStatus(id, status) {
      const o = db.orders[id];
      if (!o || o.sellerId !== me().id) throw new Error('اجازه ندارید.');
      o.status = status;
      save();
    },

    async openChat(listing) {
      const u = me();
      R.assertSameProvince(u, listing);
      if (listing.sellerId === u.id) throw new Error('این آگهی خودتان است.');
      const id = R.chatIdFor(u.id, listing.sellerId, listing.id);
      if (!db.chats[id]) {
        db.chats[id] = {
          id, members: [u.id, listing.sellerId],
          names: { [u.id]: u.name, [listing.sellerId]: listing.shopName || listing.sellerName },
          listingId: listing.id, listingTitle: listing.title, province: u.province,
          lastMessage: '', updatedAt: Date.now(), createdAt: Date.now(),
        };
        save();
      }
      return id;
    },

    async getChat(id) {
      const c = db.chats[id];
      return c && c.members.includes(me().id) ? c : null;
    },

    async myChats() {
      const u = me();
      return Object.values(db.chats)
        .filter((c) => c.members.includes(u.id))
        .sort((a, b) => b.updatedAt - a.updatedAt);
    },

    watchMessages(chatId, cb) {
      const push = () => cb(db.messages[chatId] || []);
      subs.add(push);
      push();
      return () => subs.delete(push);
    },

    async sendMessage(chatId, text) {
      const u = me();
      const c = db.chats[chatId];
      if (!c || !c.members.includes(u.id)) throw new Error('گفتگو پیدا نشد.');
      (db.messages[chatId] ||= []).push({ id: newId(), senderId: u.id, text, createdAt: Date.now() });
      c.lastMessage = text;
      c.updatedAt = Date.now();
      save();
    },

    async wallet() {
      const u = me();
      const sales = Object.values(db.orders).filter((o) => o.sellerId === u.id);
      const withdrawals = list(db.withdrawals).filter((w) => w.uid === u.id);
      return { ...R.computeWallet(sales, withdrawals), withdrawals };
    },

    async requestWithdrawal(amount) {
      const u = me();
      const { balance } = await this.wallet();
      if (!(amount > 0) || amount > balance) throw new Error('مبلغ از موجودی شما بیشتر است.');
      const id = newId();
      const code = randomCode();
      db.withdrawals[id] = { id, uid: u.id, name: u.name, phone: u.phone, amount, code, status: 'pending', createdAt: Date.now() };
      save();
      return code;
    },
  };
}
