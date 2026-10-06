import * as THREE from 'three';
import { Box, groundHeight, resolveCircle } from './physics';
import { Inventory } from './weapons';
import { World } from './world';

export const PLAYER_RADIUS = 0.4;
export const PLAYER_HEIGHT = 1.8;

/** Simple low-poly soldier made of primitives. Shared by the player and the bots. */
export function buildSoldier(bodyColor: number, headColor = 0xe0b48a): THREE.Group {
  const g = new THREE.Group();
  const lam = (c: number) => new THREE.MeshLambertMaterial({ color: c });
  const body = new THREE.Mesh(new THREE.CapsuleGeometry(0.32, 0.75, 4, 8), lam(bodyColor));
  body.position.y = 0.95;
  body.name = 'body';
  const legs = new THREE.Mesh(new THREE.BoxGeometry(0.5, 0.55, 0.3), lam(0x2d2d2d));
  legs.position.y = 0.3;
  legs.name = 'body';
  const head = new THREE.Mesh(new THREE.SphereGeometry(0.2, 10, 8), lam(headColor));
  head.position.y = 1.6;
  head.name = 'head';
  const gun = new THREE.Mesh(new THREE.BoxGeometry(0.08, 0.1, 0.7), lam(0x222222));
  gun.position.set(0.22, 1.15, -0.35);
  gun.name = 'gun';
  g.add(body, legs, head, gun);
  g.traverse((o) => { o.castShadow = true; });
  return g;
}

export class Player {
  readonly pos = new THREE.Vector3();
  yaw = 0;
  pitch = 0;
  private velY = 0;
  onGround = true;
  health = 100;
  alive = true;
  readonly inv = new Inventory();
  readonly mesh = buildSoldier(0x3d5a80);
  fireCooldown = 0;
  reloadTimer = 0;
  /** Healing in progress: seconds left and what will be applied. */
  heal: { kind: 'bandage' | 'medkit'; left: number; total: number } | null = null;
  moving = false;

  constructor(scene: THREE.Scene) {
    scene.add(this.mesh);
  }

  get eye() { return new THREE.Vector3(this.pos.x, this.pos.y + 1.6, this.pos.z); }

  forward(out = new THREE.Vector3()) { return out.set(-Math.sin(this.yaw), 0, -Math.cos(this.yaw)); }

  update(dt: number, move: { x: number; y: number }, sprint: boolean, jump: boolean, aiming: boolean, world: World) {
    const boxes: Box[] = world.boxes;
    let speed = sprint && move.y > 0 && !aiming ? 7.5 : 4.6;
    if (aiming) speed = 3;
    if (this.heal) speed = 2;
    const f = this.forward();
    const r = new THREE.Vector3(-f.z, 0, f.x);
    const dir = f.multiplyScalar(move.y).add(r.multiplyScalar(move.x));
    if (dir.lengthSq() > 1) dir.normalize();
    this.moving = dir.lengthSq() > 0.01;
    this.pos.addScaledVector(dir, speed * dt);

    if (jump && this.onGround) { this.velY = 5.2; this.onGround = false; }
    this.velY -= 16 * dt;
    this.pos.y += this.velY * dt;
    // Head bump against ceilings.
    if (this.velY > 0) {
      for (const b of boxes) {
        if (this.pos.x > b.minX && this.pos.x < b.maxX && this.pos.z > b.minZ && this.pos.z < b.maxZ &&
            this.pos.y + PLAYER_HEIGHT > b.minY && this.pos.y < b.minY) {
          this.pos.y = b.minY - PLAYER_HEIGHT;
          this.velY = 0;
        }
      }
    }
    const floor = groundHeight(this.pos.x, this.pos.z, this.pos.y, boxes);
    if (this.pos.y <= floor) { this.pos.y = floor; this.velY = 0; this.onGround = true; }
    else if (this.pos.y - floor > 0.05) this.onGround = false;

    resolveCircle(this.pos, PLAYER_RADIUS, PLAYER_HEIGHT, boxes);
    const lim = world.half - 1;
    this.pos.x = THREE.MathUtils.clamp(this.pos.x, -lim, lim);
    this.pos.z = THREE.MathUtils.clamp(this.pos.z, -lim, lim);

    this.mesh.position.copy(this.pos);
    this.mesh.rotation.y = this.yaw;

    this.fireCooldown = Math.max(0, this.fireCooldown - dt);
  }
}
