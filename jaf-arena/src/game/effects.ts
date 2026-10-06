import * as THREE from 'three';

interface Fx { obj: THREE.Mesh | THREE.Line; life: number; max: number; grow: number; }

/** Short-lived visual effects: bullet tracers, impact sparks, muzzle flashes. */
export class Effects {
  private items: Fx[] = [];
  private sparkGeo = new THREE.SphereGeometry(0.08, 6, 4);

  constructor(private scene: THREE.Scene) {}

  tracer(from: THREE.Vector3, to: THREE.Vector3, color = 0xffe9a0) {
    const geo = new THREE.BufferGeometry().setFromPoints([from, to]);
    const line = new THREE.Line(geo, new THREE.LineBasicMaterial({ color, transparent: true, opacity: 0.9 }));
    this.scene.add(line);
    this.items.push({ obj: line, life: 0.07, max: 0.07, grow: 0 });
  }

  spark(at: THREE.Vector3, color = 0xffd070, size = 1) {
    const m = new THREE.Mesh(this.sparkGeo, new THREE.MeshBasicMaterial({ color, transparent: true }));
    m.position.copy(at);
    m.scale.setScalar(size);
    this.scene.add(m);
    this.items.push({ obj: m, life: 0.15, max: 0.15, grow: 8 * size });
  }

  update(dt: number) {
    for (let i = this.items.length - 1; i >= 0; i--) {
      const f = this.items[i];
      f.life -= dt;
      const mat = f.obj.material as THREE.Material;
      mat.opacity = Math.max(0, f.life / f.max);
      if (f.grow) f.obj.scale.addScalar(f.grow * dt);
      if (f.life <= 0) {
        this.scene.remove(f.obj);
        if (f.obj instanceof THREE.Line) f.obj.geometry.dispose();
        mat.dispose();
        this.items.splice(i, 1);
      }
    }
  }
}
