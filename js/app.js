(function () {
  const CART_KEY = 'jaf-cart';
  const $ = (id) => document.getElementById(id);
  const fmt = (n) => n.toLocaleString('fa-IR');

  let cart = loadCart();
  let activeCategory = 'همه';

  function loadCart() {
    try {
      return JSON.parse(localStorage.getItem(CART_KEY)) || {};
    } catch {
      return {};
    }
  }

  function saveCart() {
    try {
      localStorage.setItem(CART_KEY, JSON.stringify(cart));
    } catch {
      // ذخیره‌سازی در دسترس نیست؛ سبد فقط در همین صفحه نگه داشته می‌شود
    }
  }

  function toast(msg) {
    const t = $('toast');
    t.textContent = msg;
    t.classList.add('show');
    clearTimeout(toast.timer);
    toast.timer = setTimeout(() => t.classList.remove('show'), 2000);
  }

  function renderCategories() {
    const cats = ['همه', ...new Set(PRODUCTS.map((p) => p.category))];
    $('categories').innerHTML = cats
      .map((c) => `<button class="chip${c === activeCategory ? ' active' : ''}" data-cat="${c}">${c}</button>`)
      .join('');
  }

  function renderProducts() {
    const q = $('search').value.trim();
    const list = PRODUCTS.filter(
      (p) => (activeCategory === 'همه' || p.category === activeCategory) && (!q || p.name.includes(q))
    );
    $('products').innerHTML = list
      .map(
        (p) => `
        <article class="card">
          <div class="card__img">${p.emoji}</div>
          <div class="card__body">
            <span class="card__cat">${p.category}</span>
            <h3>${p.name}</h3>
            <div class="card__price">${fmt(p.price)} <small>تومان</small></div>
            <button class="btn btn--primary" data-add="${p.id}">افزودن به سبد</button>
          </div>
        </article>`
      )
      .join('');
    $('noResults').hidden = list.length > 0;
  }

  function renderCart() {
    const items = Object.entries(cart)
      .map(([id, qty]) => ({ product: PRODUCTS.find((p) => p.id === Number(id)), qty }))
      .filter((i) => i.product);

    $('cartItems').innerHTML = items
      .map(
        ({ product: p, qty }) => `
        <li class="cart-item">
          <span class="cart-item__emoji">${p.emoji}</span>
          <div class="cart-item__info">
            <strong>${p.name}</strong>
            <small>${fmt(p.price * qty)} تومان</small>
          </div>
          <div class="qty">
            <button data-inc="${p.id}" aria-label="افزایش">+</button>
            <span>${fmt(qty)}</span>
            <button data-dec="${p.id}" aria-label="کاهش">−</button>
          </div>
        </li>`
      )
      .join('');

    const count = items.reduce((s, i) => s + i.qty, 0);
    const total = items.reduce((s, i) => s + i.qty * i.product.price, 0);
    $('cartCount').textContent = fmt(count);
    $('cartTotal').textContent = fmt(total);
    $('cartEmpty').hidden = items.length > 0;
    $('checkoutBtn').disabled = items.length === 0;
  }

  function changeQty(id, delta) {
    cart[id] = (cart[id] || 0) + delta;
    if (cart[id] <= 0) delete cart[id];
    saveCart();
    renderCart();
  }

  function openCart(open) {
    $('cart').classList.toggle('open', open);
    $('cart').setAttribute('aria-hidden', String(!open));
    $('overlay').hidden = !open;
  }

  // رویدادها
  $('categories').addEventListener('click', (e) => {
    const cat = e.target.dataset.cat;
    if (!cat) return;
    activeCategory = cat;
    renderCategories();
    renderProducts();
  });

  $('search').addEventListener('input', renderProducts);

  $('products').addEventListener('click', (e) => {
    const id = e.target.dataset.add;
    if (!id) return;
    changeQty(id, 1);
    toast('به سبد خرید اضافه شد ✓');
  });

  $('cartItems').addEventListener('click', (e) => {
    if (e.target.dataset.inc) changeQty(e.target.dataset.inc, 1);
    if (e.target.dataset.dec) changeQty(e.target.dataset.dec, -1);
  });

  $('cartOpen').addEventListener('click', () => openCart(true));
  $('cartClose').addEventListener('click', () => openCart(false));
  $('overlay').addEventListener('click', () => openCart(false));

  $('checkoutBtn').addEventListener('click', () => {
    openCart(false);
    $('checkout').showModal();
  });
  $('checkoutCancel').addEventListener('click', () => $('checkout').close());

  $('checkoutForm').addEventListener('submit', () => {
    // فعلاً سفارش فقط در مرورگر ثبت می‌شود؛ بعداً به سرور/درگاه پرداخت وصل می‌شود.
    cart = {};
    saveCart();
    renderCart();
    $('checkoutForm').reset();
    toast('سفارش شما با موفقیت ثبت شد 🎉');
  });

  renderCategories();
  renderProducts();
  renderCart();
})();
