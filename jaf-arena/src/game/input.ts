/** Unified input for keyboard + mouse (web/desktop) and touch screens (iPhone/Android). */
export class Input {
  readonly isTouch: boolean;
  /** Movement vector, x = right, y = forward, each -1..1. */
  move = { x: 0, y: 0 };
  /** Accumulated look delta in radians since the last `consumeLook`. */
  private look = { x: 0, y: 0 };
  fire = false;
  aim = false;
  sprint = false;
  private pressed = new Set<string>();
  private keys = new Set<string>();
  mouseSensitivity = 0.0022;
  touchSensitivity = 0.0055;

  constructor(private canvas: HTMLCanvasElement) {
    this.isTouch = matchMedia('(pointer: coarse)').matches || 'ontouchstart' in window;
    if (this.isTouch) this.bindTouch();
    else this.bindDesktop();
  }

  /** One-shot actions: fire (press latch), jump, reload, pickup, switch, bandage, medkit, slot0, slot1. */
  take(action: string): boolean {
    return this.pressed.delete(action);
  }

  consumeLook() {
    const l = { ...this.look };
    this.look.x = this.look.y = 0;
    return l;
  }

  get locked() { return document.pointerLockElement === this.canvas; }

  requestLock() {
    if (!this.isTouch && !this.locked) {
      try { void this.canvas.requestPointerLock()?.catch?.(() => {}); } catch { /* not supported */ }
    }
  }

  private bindDesktop() {
    const map: Record<string, string> = {
      Space: 'jump', KeyR: 'reload', KeyF: 'pickup', KeyE: 'pickup', Digit1: 'slot0', Digit2: 'slot1',
      Digit4: 'bandage', Digit5: 'medkit', KeyQ: 'switch',
    };
    addEventListener('keydown', (e) => {
      this.keys.add(e.code);
      if (!e.repeat && map[e.code]) this.pressed.add(map[e.code]);
      this.updateKeys();
    });
    addEventListener('keyup', (e) => { this.keys.delete(e.code); this.updateKeys(); });
    addEventListener('blur', () => { this.keys.clear(); this.fire = this.aim = false; this.updateKeys(); });
    this.canvas.addEventListener('mousedown', (e) => {
      if (!this.locked) { this.requestLock(); return; }
      if (e.button === 0) { this.fire = true; this.pressed.add('fire'); }
      if (e.button === 2) this.aim = true;
    });
    addEventListener('mouseup', (e) => {
      if (e.button === 0) this.fire = false;
      if (e.button === 2) this.aim = false;
    });
    addEventListener('contextmenu', (e) => e.preventDefault());
    addEventListener('mousemove', (e) => {
      if (!this.locked) return;
      const s = this.mouseSensitivity * (this.aim ? 0.55 : 1);
      this.look.x += e.movementX * s;
      this.look.y += e.movementY * s;
    });
    addEventListener('wheel', () => this.pressed.add('switch'));
  }

  private updateKeys() {
    const k = this.keys;
    this.move.x = (k.has('KeyD') || k.has('ArrowRight') ? 1 : 0) - (k.has('KeyA') || k.has('ArrowLeft') ? 1 : 0);
    this.move.y = (k.has('KeyW') || k.has('ArrowUp') ? 1 : 0) - (k.has('KeyS') || k.has('ArrowDown') ? 1 : 0);
    if (this.move.x && this.move.y) { this.move.x *= Math.SQRT1_2; this.move.y *= Math.SQRT1_2; }
    this.sprint = k.has('ShiftLeft') || k.has('ShiftRight');
  }

  private bindTouch() {
    document.body.classList.add('touch');
    const $ = (id: string) => document.getElementById(id)!;
    const base = $('joy-base');
    const knob = $('joy-knob');
    const R = 60;
    let joyId: number | null = null;
    let joyOrigin = { x: 0, y: 0 };
    const lookTouches = new Map<number, { x: number; y: number }>();

    $('joy-zone').addEventListener('touchstart', (e) => {
      e.preventDefault();
      if (joyId !== null) return;
      const t = e.changedTouches[0];
      joyId = t.identifier;
      const rect = base.getBoundingClientRect();
      joyOrigin = { x: rect.left + rect.width / 2, y: rect.top + rect.height / 2 };
      this.updateJoy(t.clientX - joyOrigin.x, t.clientY - joyOrigin.y, R, knob);
    }, { passive: false });

    const lookStart = (e: TouchEvent) => {
      e.preventDefault();
      for (const t of Array.from(e.changedTouches)) lookTouches.set(t.identifier, { x: t.clientX, y: t.clientY });
    };
    $('look-zone').addEventListener('touchstart', lookStart, { passive: false });

    addEventListener('touchmove', (e) => {
      for (const t of Array.from(e.changedTouches)) {
        if (t.identifier === joyId) {
          this.updateJoy(t.clientX - joyOrigin.x, t.clientY - joyOrigin.y, R, knob);
        } else if (lookTouches.has(t.identifier)) {
          const p = lookTouches.get(t.identifier)!;
          const s = this.touchSensitivity * (this.aim ? 0.5 : 1);
          this.look.x += (t.clientX - p.x) * s;
          this.look.y += (t.clientY - p.y) * s;
          p.x = t.clientX; p.y = t.clientY;
        }
      }
    }, { passive: false });

    const end = (e: TouchEvent) => {
      for (const t of Array.from(e.changedTouches)) {
        if (t.identifier === joyId) {
          joyId = null;
          this.move.x = this.move.y = 0;
          this.sprint = false;
          knob.style.transform = '';
        }
        lookTouches.delete(t.identifier);
      }
    };
    addEventListener('touchend', end);
    addEventListener('touchcancel', end);

    // Fire buttons also act as look areas while held (like popular mobile shooters).
    for (const id of ['btn-fire', 'btn-fire-left']) {
      const b = $(id);
      let held = 0;
      b.addEventListener('touchstart', (e) => {
        e.preventDefault();
        held++;
        this.fire = true;
        this.pressed.add('fire');
        lookStart(e);
      }, { passive: false });
      const up = (e: TouchEvent) => { held = Math.max(0, held - e.changedTouches.length); if (!held) this.fire = false; };
      b.addEventListener('touchend', up);
      b.addEventListener('touchcancel', up);
    }

    const aimBtn = $('btn-aim');
    aimBtn.addEventListener('touchstart', (e) => {
      e.preventDefault();
      this.aim = !this.aim;
      aimBtn.classList.toggle('on', this.aim);
    }, { passive: false });

    const tap: Record<string, string> = {
      'btn-jump': 'jump', 'btn-reload': 'reload', 'btn-switch': 'switch',
      'btn-pickup': 'pickup', 'btn-bandage': 'bandage', 'btn-medkit': 'medkit',
    };
    for (const [id, action] of Object.entries(tap)) {
      $(id).addEventListener('touchstart', (e) => { e.preventDefault(); this.pressed.add(action); }, { passive: false });
    }
  }

  private updateJoy(dx: number, dy: number, R: number, knob: HTMLElement) {
    const len = Math.hypot(dx, dy);
    if (len > R) { dx = (dx / len) * R; dy = (dy / len) * R; }
    knob.style.transform = `translate(${dx}px, ${dy}px)`;
    this.move.x = dx / R;
    this.move.y = -dy / R;
    // Push the stick all the way forward to sprint.
    this.sprint = len > R * 1.15 && this.move.y > 0.7;
  }

  /** Forget held buttons, e.g. when the aim toggle should reset after death. */
  setAim(v: boolean) {
    this.aim = v;
    document.getElementById('btn-aim')?.classList.toggle('on', v);
  }
}
