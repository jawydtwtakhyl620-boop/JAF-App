export type AmmoType = 'light' | 'heavy' | 'shell' | 'sniper';

export interface WeaponDef {
  id: string;
  name: string;
  ammo: AmmoType;
  damage: number;
  /** Seconds between shots. */
  fireInterval: number;
  magSize: number;
  reloadTime: number;
  /** Cone half-angle in radians when hip firing. */
  spread: number;
  pellets: number;
  auto: boolean;
  range: number;
  /** Camera FOV while aiming. */
  zoomFov: number;
  color: number;
  /** Pitch of the synthesized shot sound. */
  sound: number;
}

export const WEAPONS: Record<string, WeaponDef> = {
  rook:    { id: 'rook',    name: 'Rook Pistol',    ammo: 'light',  damage: 22, fireInterval: 0.22, magSize: 12, reloadTime: 1.4, spread: 0.03,  pellets: 1, auto: false, range: 120, zoomFov: 60, color: 0x444444, sound: 1.4 },
  viper:   { id: 'viper',   name: 'Viper SMG',      ammo: 'light',  damage: 17, fireInterval: 0.075, magSize: 32, reloadTime: 1.8, spread: 0.035, pellets: 1, auto: true,  range: 150, zoomFov: 55, color: 0x2f3b4a, sound: 1.2 },
  falcon:  { id: 'falcon',  name: 'Falcon AR',      ammo: 'heavy',  damage: 25, fireInterval: 0.1,  magSize: 30, reloadTime: 2.2, spread: 0.022, pellets: 1, auto: true,  range: 260, zoomFov: 45, color: 0x5a4a32, sound: 1.0 },
  hammer:  { id: 'hammer',  name: 'Hammer Shotgun', ammo: 'shell',  damage: 13, fireInterval: 0.85, magSize: 5,  reloadTime: 2.6, spread: 0.08,  pellets: 9, auto: false, range: 45,  zoomFov: 60, color: 0x6b3a22, sound: 0.6 },
  longbow: { id: 'longbow', name: 'Longbow Sniper', ammo: 'sniper', damage: 95, fireInterval: 1.4,  magSize: 5,  reloadTime: 3.0, spread: 0.004, pellets: 1, auto: false, range: 600, zoomFov: 16, color: 0x2e4a2e, sound: 0.75 },
};

export const AMMO_NAMES: Record<AmmoType, string> = {
  light: 'فشنگ سبک', heavy: 'فشنگ سنگین', shell: 'ساچمه', sniper: 'فشنگ تک‌تیرانداز',
};

export const AMMO_PACK: Record<AmmoType, number> = { light: 40, heavy: 30, shell: 10, sniper: 5 };

export interface WeaponInstance { def: WeaponDef; mag: number; }

export class Inventory {
  slots: (WeaponInstance | null)[] = [null, null];
  current = 0;
  ammo: Record<AmmoType, number> = { light: 0, heavy: 0, shell: 0, sniper: 0 };
  bandages = 0;
  medkits = 0;

  get weapon(): WeaponInstance | null { return this.slots[this.current]; }

  /** Adds a weapon; returns the weapon it replaced (to drop on the ground), if any. */
  addWeapon(def: WeaponDef, mag: number): WeaponInstance | null {
    const empty = this.slots.findIndex((s) => s === null);
    const inst = { def, mag };
    if (empty >= 0) {
      this.slots[empty] = inst;
      if (!this.slots[this.current]) this.current = empty;
      return null;
    }
    const old = this.slots[this.current];
    this.slots[this.current] = inst;
    return old;
  }
}
