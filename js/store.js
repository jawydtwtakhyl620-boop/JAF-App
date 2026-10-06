import { firebaseConfig } from './config.js';

// اگر Firebase تنظیم شده باشد از آن استفاده می‌شود، وگرنه حالت آزمایشی (ذخیره در مرورگر).
export async function createStore() {
  if (firebaseConfig.apiKey) {
    const { createFirebaseStore } = await import('./store-firebase.js');
    return createFirebaseStore(firebaseConfig);
  }
  const { createLocalStore } = await import('./store-local.js');
  return createLocalStore();
}
