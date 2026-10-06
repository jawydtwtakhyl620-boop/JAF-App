import * as THREE from 'three';
import { groundHeight, resolveCircle } from './physics';
import { PLAYER_HEIGHT, PLAYER_RADIUS, buildSoldier } from './player';
import { WEAPONS, WeaponDef } from './weapons';
import { World } from './world';

const BOT_NAMES = [
  'Shahin', 'Toofan', 'Arash', 'Sepehr', 'Kaveh', 'Babak', 'Nima', 'Omid', 'Parsa', 'Ramin',
  'Saman', 'Taha', 'Yasin', 'Zaman', 'Hamid', 'Farid', 'Jalal', 'Karim', 'Mehdi', 'Navid',
  'Rostam', 'Sohrab', 'Tahmin', 'Wahid', 'Behzad', 'Dariush', 'Elyas', 'Ghazan', 'Haris', 'Idris',
];
const BOT_COLORS = [0x8a5a3c, 0x6b6b3a, 0x7a3e3e, 0x4f6b5a, 0x6a4f7a, 0x8a7a3c];

export class Bot {
  readonly pos = new THREE.Vector3();
  readonly mesh: THREE.Group;
  yaw = Math.random() * Math.PI * 2;
  health = 100;
  alive = true;
  deadTime = 0;
  weapon: WeaponDef;
  mag: number;
  target = new THREE.Vector3();
  lastSeen: THREE.Vector3 | null = null;
  losTimer = Math.random() * 0.3;
  seesPlayer = false;
  alert = 0;
  cooldown = 0;
  reload = 0;
  strafe = Math.random() < 0.5 ? 1 : -1;
  strafeTimer = 0;
  stuck = 0;

  constructor(readonly name: string, x: number, z: number) {
    this.pos.set(x, 0, z);
    this.mesh = buildSoldier(BOT_COLORS[Math.floor(Math.random() * BOT_COLORS.length)]);
    this.mesh.traverse((o) => { o.userData.bot = this; });
    const pool = ['rook', 'viper', 'falcon', 'falcon', 'hammer', 'viper'];
    this.weapon = WEAPONS[pool[Math.floor(Math.random() * pool.length)]];
    this.mag = this.weapon.magSize;
    this.target.copy(this.pos);
  }

  /** Called when this bot is hit, so it turns towards the attacker. */
  alarm(from: THREE.Vector3) {
    this.lastSeen = from.clone();
    this.alert = Math.max(this.alert, 0.4);
  }
}

export interface BotShot { bot: Bot; from: THREE.Vector3; to: THREE.Vector3; hit: boolean; damage: number; }

export class Bots {
  readonly list: Bot[] = [];
  private ray = new THREE.Raycaster();

  constructor(private scene: THREE.Scene, private world: World, count: number, avoid: THREE.Vector3) {
    for (let i = 0; i < count; i++) {
      let s = world.randomOpenSpot();
      for (let k = 0; k < 20 && Math.hypot(s.x - avoid.x, s.z - avoid.z) < 60; k++) s = world.randomOpenSpot();
      const b = new Bot(BOT_NAMES[i % BOT_NAMES.length], s.x, s.z);
      scene.add(b.mesh);
      this.list.push(b);
      this.pickWander(b);
    }
  }

  get aliveCount() { return this.list.filter((b) => b.alive).length; }

  /** All bot meshes that can still be shot. */
  targets(): THREE.Object3D[] {
    return this.list.filter((b) => b.alive).map((b) => b.mesh);
  }

  private pickWander(b: Bot) {
    // Wander towards a random point, mostly nearby, sometimes across the map.
    const far = Math.random() < 0.25;
    const s = far ? this.world.randomOpenSpot() : {
      x: b.pos.x + (Math.random() * 2 - 1) * 40,
      z: b.pos.z + (Math.random() * 2 - 1) * 40,
    };
    const lim = this.world.half - 5;
    b.target.set(THREE.MathUtils.clamp(s.x, -lim, lim), 0, THREE.MathUtils.clamp(s.z, -lim, lim));
  }

  private lineOfSight(from: THREE.Vector3, to: THREE.Vector3): boolean {
    const dir = to.clone().sub(from);
    const dist = dir.length();
    this.ray.set(from, dir.divideScalar(dist));
    this.ray.far = dist;
    return this.ray.intersectObjects(this.world.solids, false).length === 0;
  }

  update(dt: number, playerPos: THREE.Vector3, playerAlive: boolean, playerMoving: boolean): BotShot[] {
    const shots: BotShot[] = [];
    const playerChest = new THREE.Vector3(playerPos.x, playerPos.y + 1.2, playerPos.z);
    for (const b of this.list) {
      if (!b.alive) {
        // Fall over, then sink into the ground and disappear.
        b.deadTime += dt;
        b.mesh.rotation.x = Math.max(-Math.PI / 2, b.mesh.rotation.x - dt * 5);
        if (b.deadTime > 4) b.mesh.position.y -= dt * 0.5;
        if (b.deadTime > 7 && b.mesh.parent) this.scene.remove(b.mesh);
        continue;
      }
      const eye = new THREE.Vector3(b.pos.x, b.pos.y + 1.55, b.pos.z);
      const dist = eye.distanceTo(playerChest);

      b.losTimer -= dt;
      if (b.losTimer <= 0) {
        b.losTimer = 0.25;
        b.seesPlayer = playerAlive && dist < 75 && this.lineOfSight(eye, playerChest);
        if (b.seesPlayer) b.lastSeen = playerPos.clone();
      }

      let moveDir = new THREE.Vector3();
      let speed = 3.2;
      b.cooldown -= dt;

      if (b.reload > 0) {
        b.reload -= dt;
        if (b.reload <= 0) b.mag = b.weapon.magSize;
      }

      if (b.seesPlayer) {
        b.alert = Math.min(1.5, b.alert + dt);
        const toP = playerPos.clone().sub(b.pos).setY(0);
        b.yaw = Math.atan2(-toP.x, -toP.z);
        // Keep a comfortable fighting distance and strafe sideways.
        const want = b.weapon.id === 'hammer' ? 8 : 22;
        const fwd = toP.normalize();
        b.strafeTimer -= dt;
        if (b.strafeTimer <= 0) { b.strafeTimer = 1 + Math.random() * 2; b.strafe *= -1; }
        moveDir.set(-fwd.z, 0, fwd.x).multiplyScalar(b.strafe * 0.7);
        if (dist > want + 5) moveDir.add(fwd);
        else if (dist < want - 5) moveDir.sub(fwd);
        speed = 2.6;

        // Shoot after a short reaction time.
        if (b.alert > 0.7 && b.cooldown <= 0 && b.reload <= 0 && dist < b.weapon.range) {
          if (b.mag <= 0) {
            b.reload = b.weapon.reloadTime + 0.5;
          } else {
            const burst = b.weapon.auto ? 0.16 : b.weapon.fireInterval;
            b.cooldown = burst + Math.random() * 0.25;
            b.mag--;
            let chance = 0.62 - dist / 120;
            if (playerMoving) chance -= 0.12;
            chance = THREE.MathUtils.clamp(chance, 0.06, 0.55);
            const hit = Math.random() < chance;
            const dmg = Math.round(b.weapon.damage * (b.weapon.pellets > 1 ? 3 : 1) * 0.55);
            const muzzle = eye.clone().add(fwd.clone().multiplyScalar(0.6)).setY(b.pos.y + 1.2);
            const to = playerChest.clone();
            if (!hit) to.add(new THREE.Vector3((Math.random() - 0.5) * 2, (Math.random() - 0.3) * 1.5, (Math.random() - 0.5) * 2));
            shots.push({ bot: b, from: muzzle, to, hit, damage: dmg });
          }
        }
      } else if (b.lastSeen) {
        // Investigate the last known position.
        b.alert = Math.max(0, b.alert - dt * 0.3);
        moveDir = b.lastSeen.clone().sub(b.pos).setY(0);
        if (moveDir.length() < 2) { b.lastSeen = null; this.pickWander(b); }
        speed = 4.2;
      } else {
        moveDir = b.target.clone().sub(b.pos).setY(0);
        if (moveDir.length() < 2) this.pickWander(b);
      }

      if (moveDir.lengthSq() > 0.0001) {
        moveDir.normalize();
        if (!b.seesPlayer) b.yaw = Math.atan2(-moveDir.x, -moveDir.z);
        const before = b.pos.clone();
        b.pos.addScaledVector(moveDir, speed * dt);
        resolveCircle(b.pos, PLAYER_RADIUS, PLAYER_HEIGHT, this.world.boxes);
        b.pos.y = groundHeight(b.pos.x, b.pos.z, b.pos.y, this.world.boxes);
        // Detect getting stuck on walls and choose another destination.
        if (b.pos.distanceTo(before) < speed * dt * 0.3) b.stuck += dt; else b.stuck = 0;
        if (b.stuck > 0.8) { b.stuck = 0; b.lastSeen = null; this.pickWander(b); }
      }

      b.mesh.position.copy(b.pos);
      b.mesh.rotation.y = b.yaw;
    }
    return shots;
  }
}
