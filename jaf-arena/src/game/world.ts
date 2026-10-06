import * as THREE from 'three';
import { Box, mulberry32 } from './physics';

export interface Spot { x: number; y: number; z: number; }

const WALL_H = 3.4;
const WALL_T = 0.3;
const DOOR_W = 1.8;

/** Builds the static map: ground, houses, warehouses, trees, rocks and crates. */
export class World {
  readonly half = 160; // map is 320 x 320 metres
  readonly boxes: Box[] = [];
  /** Meshes that stop bullets and block line of sight. */
  readonly solids: THREE.Object3D[] = [];
  readonly lootSpots: Spot[] = [];
  private footprints: { minX: number; maxX: number; minZ: number; maxZ: number }[] = [];
  private rng = mulberry32(20261006);

  private mat = {
    wall: [0xd8cbb0, 0xc9b79a, 0xb8c4c9, 0xd3b8a8].map((c) => new THREE.MeshLambertMaterial({ color: c })),
    roof: [0x7a3b2e, 0x4b5562, 0x5d4a3a].map((c) => new THREE.MeshLambertMaterial({ color: c })),
    floor: new THREE.MeshLambertMaterial({ color: 0x8a7560 }),
    metal: new THREE.MeshLambertMaterial({ color: 0x7d8790 }),
    crate: new THREE.MeshLambertMaterial({ color: 0x9a7444 }),
    trunk: new THREE.MeshLambertMaterial({ color: 0x5b4030 }),
    leaves: [0x3e6b35, 0x4a7a3a, 0x355c2d].map((c) => new THREE.MeshLambertMaterial({ color: c })),
    rock: new THREE.MeshLambertMaterial({ color: 0x8a8d88, flatShading: true }),
    road: new THREE.MeshLambertMaterial({ color: 0x6f6a5f }),
  };

  constructor(private scene: THREE.Scene) {
    this.buildGround();
    this.buildTowns();
    this.buildNature();
  }

  /** True when a cylinder at (x, z) would overlap any solid. */
  blocked(x: number, z: number, r = 0.6): boolean {
    for (const b of this.boxes) {
      if (b.minY > 2) continue;
      if (x > b.minX - r && x < b.maxX + r && z > b.minZ - r && z < b.maxZ + r) return true;
    }
    return Math.abs(x) > this.half - 2 || Math.abs(z) > this.half - 2;
  }

  /** Random open position on the ground. */
  randomOpenSpot(rand: () => number = Math.random, margin = 10): Spot {
    for (let i = 0; i < 200; i++) {
      const x = (rand() * 2 - 1) * (this.half - margin);
      const z = (rand() * 2 - 1) * (this.half - margin);
      if (!this.blocked(x, z, 1)) return { x, y: 0, z };
    }
    return { x: 0, y: 0, z: 0 };
  }

  private addBox(x: number, y: number, z: number, w: number, h: number, d: number, mat: THREE.Material, solid = true) {
    const mesh = new THREE.Mesh(new THREE.BoxGeometry(w, h, d), mat);
    mesh.position.set(x, y + h / 2, z);
    mesh.castShadow = true;
    mesh.receiveShadow = true;
    this.scene.add(mesh);
    if (solid) {
      this.boxes.push({ minX: x - w / 2, maxX: x + w / 2, minY: y, maxY: y + h, minZ: z - d / 2, maxZ: z + d / 2 });
      this.solids.push(mesh);
    }
    return mesh;
  }

  private buildGround() {
    // Procedural grass texture (no external image files = no copyright issues).
    const c = document.createElement('canvas');
    c.width = c.height = 128;
    const g = c.getContext('2d')!;
    g.fillStyle = '#5d7d3c';
    g.fillRect(0, 0, 128, 128);
    for (let i = 0; i < 1400; i++) {
      const v = 70 + Math.floor(this.rng() * 60);
      g.fillStyle = `rgb(${v - 10},${v + 40},${v - 30})`;
      g.fillRect(this.rng() * 128, this.rng() * 128, 2, 2);
    }
    const tex = new THREE.CanvasTexture(c);
    tex.wrapS = tex.wrapT = THREE.RepeatWrapping;
    tex.repeat.set(80, 80);
    tex.colorSpace = THREE.SRGBColorSpace;
    const size = this.half * 2 + 400;
    const ground = new THREE.Mesh(new THREE.PlaneGeometry(size, size), new THREE.MeshLambertMaterial({ map: tex }));
    ground.rotation.x = -Math.PI / 2;
    ground.receiveShadow = true;
    this.scene.add(ground);
    this.solids.push(ground);

    // Two crossing roads.
    for (const horizontal of [true, false]) {
      const road = new THREE.Mesh(new THREE.PlaneGeometry(horizontal ? this.half * 2 : 7, horizontal ? 7 : this.half * 2), this.mat.road);
      road.rotation.x = -Math.PI / 2;
      road.position.y = 0.02;
      road.receiveShadow = true;
      this.scene.add(road);
    }

    // Visible border fence so players understand where the map ends.
    const fenceMat = new THREE.MeshLambertMaterial({ color: 0x55504a });
    for (const s of [-1, 1]) {
      this.addBox(0, 0, s * this.half, this.half * 2, 2.5, 0.5, fenceMat);
      this.addBox(s * this.half, 0, 0, 0.5, 2.5, this.half * 2, fenceMat);
    }
  }

  private fits(cx: number, cz: number, w: number, d: number, margin: number) {
    const f = { minX: cx - w / 2 - margin, maxX: cx + w / 2 + margin, minZ: cz - d / 2 - margin, maxZ: cz + d / 2 + margin };
    if (Math.abs(cx) + w / 2 > this.half - 6 || Math.abs(cz) + d / 2 > this.half - 6) return false;
    // Keep the roads clear.
    if (Math.abs(cx) < w / 2 + 5 || Math.abs(cz) < d / 2 + 5) return false;
    return !this.footprints.some((o) => f.minX < o.maxX && f.maxX > o.minX && f.minZ < o.maxZ && f.maxZ > o.minZ);
  }

  private buildTowns() {
    const towns = [
      { x: -90, z: -90, n: 7 }, { x: 85, z: -95, n: 6 }, { x: -95, z: 80, n: 6 },
      { x: 90, z: 90, n: 7 }, { x: 0, z: 0, n: 8 }, { x: -20, z: -120, n: 4 },
      { x: 125, z: 10, n: 4 }, { x: -125, z: -10, n: 4 }, { x: 20, z: 120, n: 4 },
    ];
    for (const t of towns) {
      let placed = 0;
      for (let tries = 0; tries < 80 && placed < t.n; tries++) {
        const warehouse = this.rng() < 0.18;
        const w = warehouse ? 20 : 8 + Math.floor(this.rng() * 5);
        const d = warehouse ? 14 : 7 + Math.floor(this.rng() * 5);
        const x = t.x + (this.rng() * 2 - 1) * 32;
        const z = t.z + (this.rng() * 2 - 1) * 32;
        if (!this.fits(x, z, w, d, 4)) continue;
        this.footprints.push({ minX: x - w / 2, maxX: x + w / 2, minZ: z - d / 2, maxZ: z + d / 2 });
        this.buildHouse(x, z, w, d, warehouse);
        placed++;
      }
    }
  }

  /** Hollow building with door gaps, a roof and loot spots inside. */
  private buildHouse(cx: number, cz: number, w: number, d: number, warehouse: boolean) {
    const h = warehouse ? 6 : WALL_H;
    const wallMat = warehouse ? this.mat.metal : this.mat.wall[Math.floor(this.rng() * this.mat.wall.length)];
    const roofMat = this.mat.roof[Math.floor(this.rng() * this.mat.roof.length)];
    const doorSides = new Set<number>([Math.floor(this.rng() * 4)]);
    if (warehouse || this.rng() < 0.4) doorSides.add(Math.floor(this.rng() * 4));

    const floor = new THREE.Mesh(new THREE.BoxGeometry(w, 0.1, d), this.mat.floor);
    floor.position.set(cx, 0.05, cz);
    floor.receiveShadow = true;
    this.scene.add(floor);

    // side 0: north (-z), 1: south (+z), 2: west (-x), 3: east (+x)
    for (let side = 0; side < 4; side++) {
      const alongX = side < 2;
      const len = alongX ? w : d;
      const fixed = side === 0 ? cz - d / 2 : side === 1 ? cz + d / 2 : side === 2 ? cx - w / 2 : cx + w / 2;
      const center = alongX ? cx : cz;
      const segs: [number, number][] = [];
      if (doorSides.has(side)) {
        const doorW = warehouse ? 4 : DOOR_W;
        const off = (this.rng() * 2 - 1) * (len / 2 - doorW - 0.6);
        segs.push([-len / 2, off - doorW / 2], [off + doorW / 2, len / 2]);
        // Wall above the door.
        const mid = center + off;
        const lintelH = h - 2.4;
        if (alongX) this.addBox(mid, 2.4, fixed, doorW, lintelH, WALL_T, wallMat);
        else this.addBox(fixed, 2.4, mid, WALL_T, lintelH, doorW, wallMat);
      } else {
        segs.push([-len / 2, len / 2]);
      }
      for (const [a, b] of segs) {
        const sl = b - a;
        if (sl < 0.05) continue;
        const m = center + (a + b) / 2;
        if (alongX) this.addBox(m, 0, fixed, sl + WALL_T, h, WALL_T, wallMat);
        else this.addBox(fixed, 0, m, WALL_T, h, sl + WALL_T, wallMat);
      }
    }
    this.addBox(cx, h, cz, w + 0.6, 0.3, d + 0.6, roofMat);

    // Some cover / furniture inside and loot spots.
    const lootCount = warehouse ? 4 : 1 + Math.floor(this.rng() * 2);
    for (let i = 0; i < lootCount; i++) {
      this.lootSpots.push({
        x: cx + (this.rng() * 2 - 1) * (w / 2 - 1.2),
        y: 0.1,
        z: cz + (this.rng() * 2 - 1) * (d / 2 - 1.2),
      });
    }
    if (warehouse) {
      for (let i = 0; i < 3; i++) {
        const x = cx + (this.rng() * 2 - 1) * (w / 2 - 3);
        const z = cz + (this.rng() * 2 - 1) * (d / 2 - 3);
        this.addBox(x, 0, z, 1.4, 1.4, 1.4, this.mat.crate);
        this.lootSpots.push({ x, y: 1.4, z });
      }
    }
  }

  private buildNature() {
    const trunkGeo = new THREE.CylinderGeometry(0.22, 0.3, 3, 6);
    const leafGeo = new THREE.ConeGeometry(1.8, 4.5, 7);
    for (let i = 0; i < 260; i++) {
      const p = this.randomOpenSpot(this.rng, 4);
      if (this.nearFootprint(p.x, p.z, 4)) continue;
      const s = 0.8 + this.rng() * 0.6;
      const trunk = new THREE.Mesh(trunkGeo, this.mat.trunk);
      trunk.position.set(p.x, 1.5 * s, p.z);
      trunk.scale.setScalar(s);
      trunk.castShadow = true;
      this.scene.add(trunk);
      this.solids.push(trunk);
      const leaves = new THREE.Mesh(leafGeo, this.mat.leaves[i % 3]);
      leaves.position.set(p.x, (3 + 1.9) * s, p.z);
      leaves.scale.setScalar(s);
      leaves.castShadow = true;
      this.scene.add(leaves);
      const r = 0.3 * s;
      this.boxes.push({ minX: p.x - r, maxX: p.x + r, minY: 0, maxY: 3 * s, minZ: p.z - r, maxZ: p.z + r });
    }

    const rockGeo = new THREE.DodecahedronGeometry(1, 0);
    for (let i = 0; i < 70; i++) {
      const p = this.randomOpenSpot(this.rng, 4);
      if (this.nearFootprint(p.x, p.z, 3)) continue;
      const sx = 1 + this.rng() * 1.8, sy = 0.8 + this.rng() * 1.2, sz = 1 + this.rng() * 1.8;
      const rock = new THREE.Mesh(rockGeo, this.mat.rock);
      rock.position.set(p.x, sy * 0.6, p.z);
      rock.scale.set(sx, sy, sz);
      rock.rotation.y = this.rng() * Math.PI;
      rock.castShadow = rock.receiveShadow = true;
      this.scene.add(rock);
      this.solids.push(rock);
      this.boxes.push({ minX: p.x - sx * 0.8, maxX: p.x + sx * 0.8, minY: 0, maxY: sy * 1.4, minZ: p.z - sz * 0.8, maxZ: p.z + sz * 0.8 });
    }

    for (let i = 0; i < 45; i++) {
      const p = this.randomOpenSpot(this.rng, 6);
      if (this.nearFootprint(p.x, p.z, 2)) continue;
      this.addBox(p.x, 0, p.z, 1.3, 1.3, 1.3, this.mat.crate);
      if (this.rng() < 0.35) this.addBox(p.x + 1.4, 0, p.z, 1.3, 1.3, 1.3, this.mat.crate);
      if (this.rng() < 0.5) this.lootSpots.push({ x: p.x, y: 1.3, z: p.z });
    }
  }

  private nearFootprint(x: number, z: number, m: number) {
    return this.footprints.some((o) => x > o.minX - m && x < o.maxX + m && z > o.minZ - m && z < o.maxZ + m);
  }
}
