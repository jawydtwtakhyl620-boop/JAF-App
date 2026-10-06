import * as THREE from 'three';
import { AMMO_NAMES, AMMO_PACK, AmmoType, WEAPONS, WeaponDef } from './weapons';
import { Spot } from './world';

export type Item =
  | { type: 'weapon'; def: WeaponDef; mag: number }
  | { type: 'ammo'; ammo: AmmoType; amount: number }
  | { type: 'bandage'; count: number }
  | { type: 'medkit'; count: number };

export interface Pickup { item: Item; mesh: THREE.Object3D; pos: THREE.Vector3; }

const AMMO_COLORS: Record<AmmoType, number> = { light: 0xe0c040, heavy: 0x40a050, shell: 0xc04030, sniper: 0x4070c0 };

export function itemLabel(item: Item): string {
  switch (item.type) {
    case 'weapon': return item.def.name;
    case 'ammo': return `${AMMO_NAMES[item.ammo]} ×${item.amount}`;
    case 'bandage': return `باند ×${item.count}`;
    case 'medkit': return `جعبه کمک‌های اولیه ×${item.count}`;
  }
}

function weightedWeapon(r: number): WeaponDef {
  const table: [string, number][] = [['rook', 24], ['viper', 24], ['falcon', 22], ['hammer', 20], ['longbow', 10]];
  let t = r * table.reduce((s, [, w]) => s + w, 0);
  for (const [id, w] of table) { if ((t -= w) <= 0) return WEAPONS[id]; }
  return WEAPONS.rook;
}

export class Pickups {
  readonly list: Pickup[] = [];
  private ringGeo = new THREE.RingGeometry(0.35, 0.45, 20);
  private ringMat = new THREE.MeshBasicMaterial({ color: 0xffe080, transparent: true, opacity: 0.55, side: THREE.DoubleSide });

  constructor(private scene: THREE.Scene) {}

  populate(spots: Spot[]) {
    const ammoTypes: AmmoType[] = ['light', 'heavy', 'shell', 'sniper'];
    for (const s of spots) {
      const r = Math.random();
      if (r < 0.4) {
        const def = weightedWeapon(Math.random());
        this.spawn({ type: 'weapon', def, mag: def.magSize }, s.x, s.y, s.z);
        this.spawn({ type: 'ammo', ammo: def.ammo, amount: AMMO_PACK[def.ammo] }, s.x + 0.7, s.y, s.z + 0.3);
      } else if (r < 0.72) {
        const a = ammoTypes[Math.floor(Math.random() * 4)];
        this.spawn({ type: 'ammo', ammo: a, amount: AMMO_PACK[a] }, s.x, s.y, s.z);
      } else if (r < 0.9) {
        this.spawn({ type: 'bandage', count: 3 }, s.x, s.y, s.z);
      } else {
        this.spawn({ type: 'medkit', count: 1 }, s.x, s.y, s.z);
      }
    }
  }

  spawn(item: Item, x: number, y: number, z: number) {
    const mesh = this.buildMesh(item);
    mesh.position.set(x, y + 0.25, z);
    const ring = new THREE.Mesh(this.ringGeo, this.ringMat);
    ring.rotation.x = -Math.PI / 2;
    ring.position.set(x, y + 0.04, z);
    const root = new THREE.Group();
    root.add(mesh, ring);
    this.scene.add(root);
    this.list.push({ item, mesh: root, pos: new THREE.Vector3(x, y, z) });
  }

  nearest(p: THREE.Vector3, maxDist: number): Pickup | null {
    let best: Pickup | null = null;
    let bd = maxDist * maxDist;
    for (const k of this.list) {
      if (Math.abs(k.pos.y - p.y) > 1.6) continue;
      const d = (k.pos.x - p.x) ** 2 + (k.pos.z - p.z) ** 2;
      if (d < bd) { bd = d; best = k; }
    }
    return best;
  }

  remove(p: Pickup) {
    this.scene.remove(p.mesh);
    const i = this.list.indexOf(p);
    if (i >= 0) this.list.splice(i, 1);
  }

  update(t: number) {
    for (const p of this.list) p.mesh.children[0].rotation.y = t * 1.5;
  }

  private buildMesh(item: Item): THREE.Object3D {
    const g = new THREE.Group();
    const lam = (c: number) => new THREE.MeshLambertMaterial({ color: c });
    if (item.type === 'weapon') {
      const len = item.def.id === 'longbow' ? 1.3 : item.def.id === 'rook' ? 0.35 : 0.9;
      const body = new THREE.Mesh(new THREE.BoxGeometry(len, 0.14, 0.1), lam(item.def.color));
      const grip = new THREE.Mesh(new THREE.BoxGeometry(0.1, 0.22, 0.08), lam(0x222222));
      grip.position.set(-len * 0.2, -0.14, 0);
      g.add(body, grip);
    } else if (item.type === 'ammo') {
      g.add(new THREE.Mesh(new THREE.BoxGeometry(0.35, 0.25, 0.25), lam(AMMO_COLORS[item.ammo])));
    } else if (item.type === 'bandage') {
      const roll = new THREE.Mesh(new THREE.CylinderGeometry(0.12, 0.12, 0.2, 10), lam(0xf4f0e6));
      roll.rotation.z = Math.PI / 2;
      g.add(roll);
    } else {
      g.add(new THREE.Mesh(new THREE.BoxGeometry(0.45, 0.28, 0.3), lam(0xf8f8f8)));
      const red = lam(0xd02020);
      const a = new THREE.Mesh(new THREE.BoxGeometry(0.06, 0.2, 0.31), red);
      const b = new THREE.Mesh(new THREE.BoxGeometry(0.2, 0.06, 0.31), red);
      g.add(a, b);
    }
    g.traverse((o) => { o.castShadow = true; });
    return g;
  }
}
