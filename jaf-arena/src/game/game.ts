import * as THREE from 'three';
import { Sfx } from './audio';
import { Bot, Bots } from './bots';
import { Effects } from './effects';
import { Hud } from './hud';
import { Input } from './input';
import { Pickup, Pickups, itemLabel } from './pickups';
import { Player } from './player';
import { AMMO_PACK, WEAPONS } from './weapons';
import { World } from './world';

export const BOT_COUNT = 24;

export interface MatchResult { win: boolean; kills: number; place: number; }

/** One match: owns the scene, runs the loop, applies the game rules. */
export class Game {
  private renderer: THREE.WebGLRenderer;
  private scene = new THREE.Scene();
  private camera = new THREE.PerspectiveCamera(70, 1, 0.1, 600);
  private sun = new THREE.DirectionalLight(0xfff2dd, 2.2);
  private world: World;
  private player: Player;
  private bots: Bots;
  private pickups: Pickups;
  private effects: Effects;
  private hud = new Hud();
  private clock = new THREE.Clock();
  private ray = new THREE.Raycaster();
  private kills = 0;
  private firedThisPress = false;
  private running = false;
  private camDist = 3.4;
  private time = 0;

  constructor(canvas: HTMLCanvasElement, private input: Input, private sfx: Sfx, private onEnd: (r: MatchResult) => void) {
    const mobile = input.isTouch;
    this.renderer = new THREE.WebGLRenderer({ canvas, antialias: !mobile, powerPreference: 'high-performance' });
    this.renderer.setPixelRatio(Math.min(devicePixelRatio, mobile ? 1.5 : 2));
    this.renderer.shadowMap.enabled = true;
    this.renderer.shadowMap.type = THREE.PCFShadowMap;

    const sky = 0xa9cbe6;
    this.scene.background = new THREE.Color(sky);
    this.scene.fog = new THREE.Fog(sky, 70, 280);
    this.scene.add(new THREE.HemisphereLight(0xdfefff, 0x5a6a40, 1.1));
    this.sun.position.set(60, 100, 40);
    this.sun.castShadow = true;
    const sc = this.sun.shadow.camera;
    sc.left = sc.bottom = -45;
    sc.right = sc.top = 45;
    sc.far = 300;
    this.sun.shadow.mapSize.setScalar(mobile ? 1024 : 2048);
    this.scene.add(this.sun, this.sun.target);

    this.world = new World(this.scene);
    this.effects = new Effects(this.scene);
    this.pickups = new Pickups(this.scene);
    this.pickups.populate(this.world.lootSpots);

    this.player = new Player(this.scene);
    const spawn = this.world.randomOpenSpot();
    this.player.pos.set(spawn.x, 0, spawn.z);
    this.player.yaw = Math.random() * Math.PI * 2;
    this.player.inv.addWeapon(WEAPONS.rook, WEAPONS.rook.magSize);
    this.player.inv.ammo.light = 24;

    this.bots = new Bots(this.scene, this.world, BOT_COUNT, this.player.pos);

    addEventListener('resize', this.resize);
    this.resize();
  }

  private resize = () => {
    const w = innerWidth, h = innerHeight;
    this.renderer.setSize(w, h, false);
    this.camera.aspect = w / h;
    this.camera.updateProjectionMatrix();
  };

  start() {
    this.running = true;
    this.clock.start();
    // Exposed in development only, for automated tests.
    if (import.meta.env.DEV) (window as unknown as { __game: Game }).__game = this;
    this.renderer.setAnimationLoop(this.frame);
  }

  dispose() {
    this.running = false;
    this.renderer.setAnimationLoop(null);
    removeEventListener('resize', this.resize);
    this.renderer.dispose();
  }

  private frame = () => {
    const dt = Math.min(this.clock.getDelta(), 0.05);
    this.time += dt;
    if (this.running) this.update(dt);
    this.effects.update(dt);
    this.pickups.update(this.time);
    this.renderer.render(this.scene, this.camera);
  };

  private update(dt: number) {
    const p = this.player;
    const inp = this.input;
    const aiming = inp.aim && !!p.inv.weapon;

    const look = inp.consumeLook();
    p.yaw -= look.x;
    p.pitch = THREE.MathUtils.clamp(p.pitch - look.y, -1.2, 1.1);
    p.update(dt, inp.move, inp.sprint, inp.take('jump'), aiming, this.world);

    this.handleActions(dt);
    this.updateCamera(dt, aiming);
    this.handleFire();

    for (const shot of this.bots.update(dt, p.pos, p.alive, p.moving)) {
      this.effects.tracer(shot.from, shot.to, 0xffb080);
      const d = shot.from.distanceTo(p.pos);
      this.sfx.shot(shot.bot.weapon.sound, 0.7 * (1 - d / 160));
      if (shot.hit && p.alive) this.damagePlayer(shot.damage, shot.bot);
    }

    // Keep the shadow area centred on the player.
    this.sun.position.set(p.pos.x + 60, 100, p.pos.z + 40);
    this.sun.target.position.copy(p.pos);

    const near = this.pickups.nearest(p.pos, 2.2);
    const key = inp.isTouch ? '✋' : 'F';
    this.hud.update(dt, p, this.bots.aliveCount + (p.alive ? 1 : 0), this.kills, near ? `${key}: برداشتن ${itemLabel(near.item)}` : null);
    const spread = (p.inv.weapon?.def.spread ?? 0.02) * this.spreadFactor(aiming) * 300;
    this.hud.crosshairSpread(spread);
  }

  private spreadFactor(aiming: boolean) {
    const p = this.player;
    return (aiming ? 0.35 : 1) * (p.moving ? 1.6 : 1) * (p.onGround ? 1 : 2.5);
  }

  private updateCamera(dt: number, aiming: boolean) {
    const p = this.player;
    const w = p.inv.weapon;
    const targetFov = aiming && w ? w.def.zoomFov : 70;
    this.camera.fov += (targetFov - this.camera.fov) * Math.min(1, dt * 12);
    this.camera.updateProjectionMatrix();

    const cp = Math.cos(p.pitch);
    const dir = new THREE.Vector3(-Math.sin(p.yaw) * cp, Math.sin(p.pitch), -Math.cos(p.yaw) * cp);
    const right = new THREE.Vector3(Math.cos(p.yaw), 0, -Math.sin(p.yaw));
    const pivot = new THREE.Vector3(p.pos.x, p.pos.y + 1.6, p.pos.z);
    const side = aiming ? 0.55 : 0.75;
    const back = aiming ? 1.6 : 3.4;
    const shoulder = pivot.clone().addScaledVector(right, side);

    // Pull the camera in when a wall is between it and the player.
    this.ray.set(shoulder, dir.clone().negate());
    this.ray.near = 0;
    this.ray.far = back;
    const hit = this.ray.intersectObjects(this.world.solids, false)[0];
    const wanted = hit ? Math.max(0.3, hit.distance - 0.25) : back;
    this.camDist = wanted < this.camDist ? wanted : this.camDist + (wanted - this.camDist) * Math.min(1, dt * 6);

    this.camera.position.copy(shoulder).addScaledVector(dir, -this.camDist).add(new THREE.Vector3(0, 0.25, 0));
    this.camera.lookAt(this.camera.position.clone().add(dir));
    // Hide the own character when the camera is squeezed into it.
    p.mesh.visible = this.camDist > 0.8;
  }

  private handleActions(dt: number) {
    const p = this.player;
    const inp = this.input;
    const inv = p.inv;
    if (!p.alive) return;

    // Weapon switching cancels reload.
    let slot = -1;
    if (inp.take('slot0')) slot = 0;
    if (inp.take('slot1')) slot = 1;
    if (inp.take('switch')) slot = 1 - inv.current;
    if (slot >= 0 && slot !== inv.current && inv.slots[slot]) {
      inv.current = slot;
      p.reloadTimer = 0;
      p.fireCooldown = 0.3;
    }

    const w = inv.weapon;
    if (inp.take('reload')) this.startReload();
    if (p.reloadTimer > 0 && w) {
      p.reloadTimer -= dt;
      if (p.reloadTimer <= 0) {
        const n = Math.min(w.def.magSize - w.mag, inv.ammo[w.def.ammo]);
        w.mag += n;
        inv.ammo[w.def.ammo] -= n;
        p.reloadTimer = 0;
      }
    }

    // Healing.
    if (inp.take('bandage') && !p.heal && inv.bandages > 0 && p.health < 75) p.heal = { kind: 'bandage', left: 3, total: 3 };
    if (inp.take('medkit') && !p.heal && inv.medkits > 0 && p.health < 100) p.heal = { kind: 'medkit', left: 6, total: 6 };
    if (p.heal) {
      p.heal.left -= dt;
      if (p.heal.left <= 0) {
        if (p.heal.kind === 'bandage') { inv.bandages--; p.health = Math.min(75, p.health + 15); }
        else { inv.medkits--; p.health = 100; }
        p.heal = null;
        this.sfx.pickup();
      }
    }

    // Ammo and healing items are collected by walking over them; weapons need the button.
    const close = this.pickups.nearest(p.pos, 1.3);
    if (close && close.item.type !== 'weapon') this.collect(close);
    if (inp.take('pickup')) {
      const near = this.pickups.nearest(p.pos, 2.2);
      if (near) this.collect(near);
    }
  }

  private collect(k: Pickup) {
    const p = this.player;
    const it = k.item;
    this.pickups.remove(k);
    this.sfx.pickup();
    switch (it.type) {
      case 'weapon': {
        const old = p.inv.addWeapon(it.def, it.mag);
        p.reloadTimer = 0;
        if (old) this.pickups.spawn({ type: 'weapon', def: old.def, mag: old.mag }, p.pos.x, p.pos.y, p.pos.z);
        break;
      }
      case 'ammo': p.inv.ammo[it.ammo] += it.amount; break;
      case 'bandage': p.inv.bandages += it.count; break;
      case 'medkit': p.inv.medkits += it.count; break;
    }
  }

  private startReload() {
    const p = this.player;
    const w = p.inv.weapon;
    if (!w || p.reloadTimer > 0 || w.mag >= w.def.magSize || p.inv.ammo[w.def.ammo] <= 0) return;
    p.reloadTimer = w.def.reloadTime;
    p.heal = null;
    this.sfx.reload();
  }

  private handleFire() {
    const p = this.player;
    const inp = this.input;
    const w = p.inv.weapon;
    // A quick tap can start and end between two frames, so presses are latched too.
    const tapped = inp.take('fire');
    if (tapped) this.firedThisPress = false;
    const fire = inp.fire || tapped;
    if (!inp.fire && !tapped) this.firedThisPress = false;
    if (!w || !p.alive || !fire || p.reloadTimer > 0 || p.fireCooldown > 0) return;
    if (!w.def.auto && this.firedThisPress) return;
    if (!inp.isTouch && !inp.locked) return;
    this.firedThisPress = true;

    if (w.mag <= 0) {
      this.sfx.empty();
      this.startReload();
      p.fireCooldown = 0.25;
      return;
    }
    w.mag--;
    p.fireCooldown = w.def.fireInterval;
    p.heal = null;
    this.sfx.shot(w.def.sound, 1);

    const aiming = inp.aim;
    const spread = w.def.spread * this.spreadFactor(aiming);
    const camDir = new THREE.Vector3();
    this.camera.getWorldDirection(camDir);
    const origin = this.camera.position.clone();
    const muzzle = p.pos.clone().add(new THREE.Vector3(0, 1.25, 0)).addScaledVector(p.forward(), 0.7)
      .addScaledVector(new THREE.Vector3(Math.cos(p.yaw), 0, -Math.sin(p.yaw)), 0.25);
    const targets = [...this.bots.targets(), ...this.world.solids];
    const damaged = new Map<Bot, { dmg: number; head: boolean }>();

    for (let i = 0; i < w.def.pellets; i++) {
      const d = camDir.clone();
      d.x += (Math.random() * 2 - 1) * spread;
      d.y += (Math.random() * 2 - 1) * spread;
      d.z += (Math.random() * 2 - 1) * spread;
      d.normalize();
      // Start the ray at the player so walls behind the character are ignored.
      this.ray.set(origin, d);
      this.ray.near = this.camDist + 0.4;
      this.ray.far = w.def.range;
      const hit = this.ray.intersectObjects(targets, true)[0];
      const end = hit ? hit.point : origin.clone().addScaledVector(d, w.def.range);
      if (i < 3) this.effects.tracer(muzzle, end);
      if (!hit) continue;
      const bot = hit.object.userData.bot as Bot | undefined;
      if (bot && bot.alive) {
        const head = hit.object.name === 'head';
        const falloff = hit.distance > w.def.range * 0.5 ? 0.75 : 1;
        const e = damaged.get(bot) ?? { dmg: 0, head: false };
        e.dmg += w.def.damage * (head ? 2.2 : 1) * falloff;
        e.head ||= head;
        damaged.set(bot, e);
        this.effects.spark(hit.point, 0xc02020, 1.2);
      } else {
        this.effects.spark(hit.point);
      }
    }

    for (const [bot, { dmg, head }] of damaged) {
      bot.health -= dmg;
      bot.alarm(p.pos);
      this.hud.hitMarker(head);
      this.sfx.hit(head);
      if (bot.health <= 0) this.killBot(bot, head);
    }

    // Recoil kicks the view up a little.
    const kick = w.def.id === 'longbow' ? 0.05 : w.def.pellets > 1 ? 0.04 : 0.012;
    p.pitch = Math.min(1.1, p.pitch + kick * (aiming ? 0.6 : 1));
    p.yaw += (Math.random() - 0.5) * kick * 0.5;
  }

  private killBot(bot: Bot, head: boolean) {
    bot.alive = false;
    this.kills++;
    this.hud.feed(`شما ← ${bot.name}${head ? ' (هدشات)' : ''}`);
    const { x, y, z } = bot.pos;
    this.pickups.spawn({ type: 'ammo', ammo: bot.weapon.ammo, amount: AMMO_PACK[bot.weapon.ammo] }, x + 0.5, y, z);
    if (Math.random() < 0.5) this.pickups.spawn({ type: 'weapon', def: bot.weapon, mag: bot.weapon.magSize }, x - 0.5, y, z);
    if (Math.random() < 0.4) this.pickups.spawn({ type: 'bandage', count: 2 }, x, y, z + 0.6);
    if (this.bots.aliveCount === 0) this.finish(true);
  }

  private damagePlayer(dmg: number, bot: Bot) {
    const p = this.player;
    p.health -= dmg;
    this.hud.damage();
    this.sfx.hurt();
    if (p.health <= 0) {
      p.health = 0;
      p.alive = false;
      p.mesh.rotation.x = -Math.PI / 2;
      this.hud.feed(`${bot.name} ← شما`);
      this.finish(false);
    }
  }

  private finish(win: boolean) {
    this.running = false;
    this.input.fire = false;
    this.input.setAim(false);
    const place = win ? 1 : this.bots.aliveCount + 1;
    setTimeout(() => this.onEnd({ win, kills: this.kills, place }), win ? 600 : 1500);
  }
}
