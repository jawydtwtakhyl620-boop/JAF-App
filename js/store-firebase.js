// حالت واقعی: Firebase Auth + Firestore + Storage
import { initializeApp } from 'https://www.gstatic.com/firebasejs/10.12.2/firebase-app.js';
import {
  getAuth, onAuthStateChanged, createUserWithEmailAndPassword, signInWithEmailAndPassword, signOut,
} from 'https://www.gstatic.com/firebasejs/10.12.2/firebase-auth.js';
import {
  getFirestore, doc, getDoc, setDoc, addDoc, updateDoc, collection, query, where, orderBy, limit,
  getDocs, onSnapshot,
} from 'https://www.gstatic.com/firebasejs/10.12.2/firebase-firestore.js';
import { getStorage, ref, uploadBytes, getDownloadURL } from 'https://www.gstatic.com/firebasejs/10.12.2/firebase-storage.js';
import { randomCode } from './util.js';
import * as R from './rules.js';

// Firebase با ایمیل کار می‌کند؛ شمارهٔ تلفن را به یک ایمیل داخلی تبدیل می‌کنیم.
const phoneEmail = (phone) => phone.replace('+', '') + '@phone.jafx.app';

const AUTH_ERRORS = {
  'auth/email-already-in-use': 'این شماره قبلاً ثبت شده است. وارد حساب شوید.',
  'auth/invalid-credential': 'شماره یا رمز اشتباه است.',
  'auth/wrong-password': 'شماره یا رمز اشتباه است.',
  'auth/user-not-found': 'شماره یا رمز اشتباه است.',
  'auth/weak-password': 'رمز باید حداقل ۶ حرف باشد.',
  'auth/network-request-failed': 'اتصال انترنت برقرار نیست.',
  'auth/too-many-requests': 'تلاش‌های زیاد. چند دقیقه بعد دوباره امتحان کنید.',
  'permission-denied': 'اجازهٔ این کار را ندارید.',
};
const friendly = (e) => new Error(AUTH_ERRORS[e?.code] || e?.message || 'خطایی رخ داد.');
const wrap = (fn) => async (...a) => {
  try {
    return await fn(...a);
  } catch (e) {
    throw e.code ? friendly(e) : e;
  }
};
const docs = (snap) => snap.docs.map((d) => ({ id: d.id, ...d.data() }));
const newest = (a, b) => b.createdAt - a.createdAt;

export function createFirebaseStore(config) {
  const app = initializeApp(config);
  const auth = getAuth(app);
  const db = getFirestore(app);
  const storage = getStorage(app);
  let profile = null;
  let authCb = () => {};
  let registering = false;

  async function loadProfile(uid) {
    const [pub, priv] = await Promise.all([
      getDoc(doc(db, 'users', uid)),
      getDoc(doc(db, 'users', uid, 'private', 'kyc')),
    ]);
    if (!pub.exists()) return null;
    return { id: uid, ...pub.data(), address: priv.exists() ? priv.data().address : '' };
  }

  const me = () => {
    if (!profile) throw new Error('لطفاً اول وارد حساب شوید.');
    return profile;
  };

  async function upload(path, blob) {
    const r = ref(storage, path);
    await uploadBytes(r, blob, { contentType: 'image/jpeg' });
    return path;
  }

  const store = {
    mode: 'firebase',
    onlinePayment: false, // تا وصل شدن درگاه پرداخت (HesabPay / M-Paisa) فقط پرداخت هنگام تحویل

    onAuth(cb) {
      authCb = cb;
      onAuthStateChanged(auth, async (u) => {
        if (registering) return; // register() خودش پس از ساختن پروفایل خبر می‌دهد
        profile = u ? await loadProfile(u.uid).catch(() => null) : null;
        cb(profile);
      });
    },
    me: () => profile,

    async register(f) {
      registering = true;
      try {
        const cred = await createUserWithEmailAndPassword(auth, phoneEmail(f.phone), f.password);
        const uid = cred.user.uid;
        const tazkiraPath = await upload(`tazkira/${uid}/${Date.now()}.jpg`, f.tazkira.blob);
        const pub = {
          name: f.name, shopName: f.shopName, role: f.role, phone: f.phone,
          province: f.province, abroad: f.abroad, country: f.country, verified: false, createdAt: Date.now(),
        };
        await setDoc(doc(db, 'users', uid), pub);
        await setDoc(doc(db, 'users', uid, 'private', 'kyc'), {
          tazkiraNumber: f.tazkiraNumber, tazkiraPath, address: f.address,
        });
        profile = { id: uid, ...pub, address: f.address };
      } finally {
        registering = false;
      }
      authCb(profile);
    },

    async login(phone, password) {
      await signInWithEmailAndPassword(auth, phoneEmail(phone), password);
    },

    async logout() {
      await signOut(auth);
    },

    async listListings() {
      const u = me();
      const snap = await getDocs(query(
        collection(db, 'listings'),
        where('province', '==', u.province),
        where('status', '==', 'active'),
        limit(300),
      ));
      return docs(snap).sort(newest);
    },

    async getListing(id) {
      const s = await getDoc(doc(db, 'listings', id)).catch(() => null);
      if (!s?.exists()) return null;
      const l = { id: s.id, ...s.data() };
      R.assertSameProvince(me(), l);
      return l;
    },

    async myListings() {
      const snap = await getDocs(query(collection(db, 'listings'), where('sellerId', '==', me().id)));
      return docs(snap).sort(newest);
    },

    async createListing(d, image) {
      const u = me();
      R.assertCanSell(u);
      R.validateListing(d);
      let imageUrl = '';
      if (image) {
        const path = await upload(`listings/${u.id}/${Date.now()}.jpg`, image.blob);
        imageUrl = await getDownloadURL(ref(storage, path));
      }
      const r = await addDoc(collection(db, 'listings'), {
        ...d, imageUrl,
        sellerId: u.id, sellerName: u.name, sellerRole: u.role, shopName: u.shopName, sellerPhone: u.phone,
        province: u.province, status: 'active', createdAt: Date.now(),
      });
      return r.id;
    },

    async setListingStatus(id, status) {
      await updateDoc(doc(db, 'listings', id), { status });
    },

    async createOrder(listingId, delivery, payMethod) {
      const u = me();
      const l = await store.getListing(listingId);
      if (!l) throw new Error('آگهی پیدا نشد.');
      R.assertCanBuyOnline(u, l);
      R.validateDelivery(delivery);
      if (payMethod !== 'cod') throw new Error('پرداخت آنلاین هنوز فعال نیست.');
      const r = await addDoc(collection(db, 'orders'), {
        listingId, title: l.title, price: l.price, imageUrl: l.imageUrl, category: l.category,
        buyerId: u.id, buyerName: u.name, sellerId: l.sellerId, shopName: l.shopName || l.sellerName,
        province: l.province, delivery, payMethod, paid: false, status: 'pending', createdAt: Date.now(),
      });
      return r.id;
    },

    // «پرداخت شد» را فقط سرور درگاه پرداخت (Cloud Function) ثبت می‌کند، نه خود اپ.
    async payOrder() {
      throw new Error('درگاه پرداخت آنلاین هنوز وصل نشده است.');
    },

    async myOrders() {
      const uid = me().id;
      const [b, s] = await Promise.all([
        getDocs(query(collection(db, 'orders'), where('buyerId', '==', uid))),
        getDocs(query(collection(db, 'orders'), where('sellerId', '==', uid))),
      ]);
      return { buying: docs(b).sort(newest), selling: docs(s).sort(newest) };
    },

    async updateOrderStatus(id, status) {
      await updateDoc(doc(db, 'orders', id), { status });
    },

    async openChat(listing) {
      const u = me();
      R.assertSameProvince(u, listing);
      if (listing.sellerId === u.id) throw new Error('این آگهی خودتان است.');
      const id = R.chatIdFor(u.id, listing.sellerId, listing.id);
      const r = doc(db, 'chats', id);
      if (!(await getDoc(r)).exists()) {
        await setDoc(r, {
          members: [u.id, listing.sellerId],
          names: { [u.id]: u.name, [listing.sellerId]: listing.shopName || listing.sellerName },
          listingId: listing.id, listingTitle: listing.title, province: u.province,
          lastMessage: '', updatedAt: Date.now(), createdAt: Date.now(),
        });
      }
      return id;
    },

    async getChat(id) {
      const s = await getDoc(doc(db, 'chats', id)).catch(() => null);
      return s?.exists() ? { id: s.id, ...s.data() } : null;
    },

    async myChats() {
      const snap = await getDocs(query(collection(db, 'chats'), where('members', 'array-contains', me().id)));
      return docs(snap).sort((a, b) => b.updatedAt - a.updatedAt);
    },

    watchMessages(chatId, cb) {
      return onSnapshot(
        query(collection(db, 'chats', chatId, 'messages'), orderBy('createdAt')),
        (snap) => cb(docs(snap)),
        () => cb([]),
      );
    },

    async sendMessage(chatId, text) {
      const now = Date.now();
      await addDoc(collection(db, 'chats', chatId, 'messages'), { senderId: me().id, text, createdAt: now });
      await updateDoc(doc(db, 'chats', chatId), { lastMessage: text, updatedAt: now });
    },

    async wallet() {
      const uid = me().id;
      const [s, w] = await Promise.all([
        getDocs(query(collection(db, 'orders'), where('sellerId', '==', uid))),
        getDocs(query(collection(db, 'withdrawals'), where('uid', '==', uid))),
      ]);
      const withdrawals = docs(w).sort(newest);
      return { ...R.computeWallet(docs(s), withdrawals), withdrawals };
    },

    async requestWithdrawal(amount) {
      const u = me();
      const { balance } = await store.wallet();
      if (!(amount > 0) || amount > balance) throw new Error('مبلغ از موجودی شما بیشتر است.');
      const code = randomCode();
      await addDoc(collection(db, 'withdrawals'), {
        uid: u.id, name: u.name, phone: u.phone, amount, code, status: 'pending', createdAt: Date.now(),
      });
      return code;
    },
  };

  for (const k of Object.keys(store)) {
    if (typeof store[k] === 'function' && !['onAuth', 'me', 'watchMessages'].includes(k)) store[k] = wrap(store[k]);
  }
  return store;
}
