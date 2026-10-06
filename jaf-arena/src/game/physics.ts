import * as THREE from 'three';

/** Axis-aligned solid box used for movement collision. */
export interface Box {
  minX: number; maxX: number;
  minY: number; maxY: number;
  minZ: number; maxZ: number;
}

export const clamp = (v: number, a: number, b: number) => (v < a ? a : v > b ? b : v);

/** Push a vertical cylinder (feet at pos.y) out of every box it overlaps. */
export function resolveCircle(pos: THREE.Vector3, radius: number, height: number, boxes: Box[]): boolean {
  let hit = false;
  for (const b of boxes) {
    if (pos.y >= b.maxY - 0.01 || pos.y + height <= b.minY) continue;
    if (pos.x < b.minX - radius || pos.x > b.maxX + radius || pos.z < b.minZ - radius || pos.z > b.maxZ + radius) continue;
    const cx = clamp(pos.x, b.minX, b.maxX);
    const cz = clamp(pos.z, b.minZ, b.maxZ);
    const dx = pos.x - cx, dz = pos.z - cz;
    const d2 = dx * dx + dz * dz;
    if (d2 >= radius * radius) continue;
    hit = true;
    if (d2 > 1e-8) {
      const d = Math.sqrt(d2);
      pos.x += (dx / d) * (radius - d);
      pos.z += (dz / d) * (radius - d);
    } else {
      // Centre is inside the box: leave through the closest face.
      const left = pos.x - b.minX, right = b.maxX - pos.x, back = pos.z - b.minZ, front = b.maxZ - pos.z;
      const m = Math.min(left, right, back, front);
      if (m === left) pos.x = b.minX - radius;
      else if (m === right) pos.x = b.maxX + radius;
      else if (m === back) pos.z = b.minZ - radius;
      else pos.z = b.maxZ + radius;
    }
  }
  return hit;
}

/** Highest surface under (x, z) that is not above `y + step`. */
export function groundHeight(x: number, z: number, y: number, boxes: Box[], step = 0.35): number {
  let h = 0;
  for (const b of boxes) {
    if (x < b.minX || x > b.maxX || z < b.minZ || z > b.maxZ) continue;
    if (b.maxY <= y + step && b.maxY > h) h = b.maxY;
  }
  return h;
}

/** Deterministic random generator so every match uses the same map. */
export function mulberry32(seed: number) {
  return () => {
    seed |= 0; seed = (seed + 0x6d2b79f5) | 0;
    let t = Math.imul(seed ^ (seed >>> 15), 1 | seed);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}
