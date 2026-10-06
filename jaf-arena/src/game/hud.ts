import { Player } from './player';

const $ = (id: string) => document.getElementById(id)!;

/** Updates the HTML overlay. Writes to the DOM only when a value changes. */
export class Hud {
  private cache = new Map<string, string>();
  private hitTimer = 0;
  private dmgTimer = 0;

  private set(id: string, text: string) {
    if (this.cache.get(id) === text) return;
    this.cache.set(id, text);
    $(id).textContent = text;
  }

  update(dt: number, p: Player, alive: number, kills: number, prompt: string | null) {
    const hp = Math.max(0, Math.ceil(p.health));
    this.set('health-text', String(hp));
    const fill = $('health-fill');
    fill.style.width = `${hp}%`;
    fill.classList.toggle('low', hp <= 30);
    this.set('alive', String(alive));
    this.set('kills', String(kills));

    const w = p.inv.weapon;
    this.set('weapon-name', w ? w.def.name + (p.reloadTimer > 0 ? ' (پر کردن…)' : '') : 'بدون سلاح');
    this.set('ammo-mag', w ? String(w.mag) : '-');
    this.set('ammo-reserve', w ? String(p.inv.ammo[w.def.ammo]) : '-');
    for (let i = 0; i < 2; i++) {
      const el = $(`slot${i}`);
      el.classList.toggle('active', p.inv.current === i && !!p.inv.slots[i]);
      el.style.opacity = p.inv.slots[i] ? '1' : '0.35';
    }
    this.set('bandages', String(p.inv.bandages));
    this.set('medkits', String(p.inv.medkits));

    const pr = $('prompt');
    pr.classList.toggle('hidden', !prompt);
    if (prompt) this.set('prompt', prompt);

    const prog = $('progress');
    const busy = p.heal ?? (p.reloadTimer > 0 && w ? { left: p.reloadTimer, total: w.def.reloadTime, kind: 'reload' } : null);
    prog.classList.toggle('hidden', !busy);
    if (busy) {
      $('progress-fill').style.width = `${(1 - busy.left / busy.total) * 100}%`;
      const label = busy.kind === 'bandage' ? 'استفاده از باند' : busy.kind === 'medkit' ? 'استفاده از جعبه کمک' : 'پر کردن خشاب';
      this.set('progress-text', `${label} ${busy.left.toFixed(1)}`);
    }

    if (this.hitTimer > 0 && (this.hitTimer -= dt) <= 0) $('hitmarker').classList.remove('show', 'head');
    if (this.dmgTimer > 0 && (this.dmgTimer -= dt) <= 0) $('damage-flash').classList.remove('show');
  }

  hitMarker(head: boolean) {
    const el = $('hitmarker');
    el.classList.add('show');
    el.classList.toggle('head', head);
    this.hitTimer = 0.12;
  }

  damage() {
    $('damage-flash').classList.add('show');
    this.dmgTimer = 0.1;
  }

  feed(text: string) {
    const box = $('killfeed');
    const row = document.createElement('div');
    row.textContent = text;
    box.prepend(row);
    while (box.children.length > 5) box.lastChild!.remove();
    setTimeout(() => row.remove(), 6000);
  }

  crosshairSpread(px: number) {
    const spans = $('crosshair').children as HTMLCollectionOf<HTMLElement>;
    const o = Math.round(6 + px);
    spans[0].style.top = `${-o - 8}px`;
    spans[1].style.top = `${o}px`;
    spans[2].style.left = `${-o - 8}px`;
    spans[3].style.left = `${o}px`;
  }
}
