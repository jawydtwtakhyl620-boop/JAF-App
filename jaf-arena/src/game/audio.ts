/** Sound effects synthesized at runtime with WebAudio, so the game ships with no audio files. */
export class Sfx {
  private ctx: AudioContext | null = null;
  private noise: AudioBuffer | null = null;

  /** Must be called from a user gesture (browsers block audio before that). */
  unlock() {
    if (this.ctx) { void this.ctx.resume(); return; }
    const Ctor = window.AudioContext ?? (window as unknown as { webkitAudioContext: typeof AudioContext }).webkitAudioContext;
    if (!Ctor) return;
    this.ctx = new Ctor();
    const len = this.ctx.sampleRate * 0.5;
    this.noise = this.ctx.createBuffer(1, len, this.ctx.sampleRate);
    const data = this.noise.getChannelData(0);
    for (let i = 0; i < len; i++) data[i] = Math.random() * 2 - 1;
  }

  /** Gunshot: filtered noise burst plus a low thump. `volume` 0..1. */
  shot(pitch: number, volume = 1) {
    const ctx = this.ctx;
    if (!ctx || !this.noise || volume < 0.02) return;
    const t = ctx.currentTime;
    const src = ctx.createBufferSource();
    src.buffer = this.noise;
    src.playbackRate.value = pitch;
    const filter = ctx.createBiquadFilter();
    filter.type = 'lowpass';
    filter.frequency.value = 2500 * pitch;
    const gain = ctx.createGain();
    const dur = 0.22 / pitch + 0.05;
    gain.gain.setValueAtTime(0.45 * volume, t);
    gain.gain.exponentialRampToValueAtTime(0.001, t + dur);
    src.connect(filter).connect(gain).connect(ctx.destination);
    src.start(t);
    src.stop(t + dur);

    const osc = ctx.createOscillator();
    osc.frequency.setValueAtTime(140 * pitch, t);
    osc.frequency.exponentialRampToValueAtTime(40, t + 0.12);
    const og = ctx.createGain();
    og.gain.setValueAtTime(0.35 * volume, t);
    og.gain.exponentialRampToValueAtTime(0.001, t + 0.14);
    osc.connect(og).connect(ctx.destination);
    osc.start(t);
    osc.stop(t + 0.15);
  }

  private beep(freq: number, dur: number, vol: number, type: OscillatorType = 'square') {
    const ctx = this.ctx;
    if (!ctx) return;
    const t = ctx.currentTime;
    const osc = ctx.createOscillator();
    osc.type = type;
    osc.frequency.value = freq;
    const g = ctx.createGain();
    g.gain.setValueAtTime(vol, t);
    g.gain.exponentialRampToValueAtTime(0.001, t + dur);
    osc.connect(g).connect(ctx.destination);
    osc.start(t);
    osc.stop(t + dur);
  }

  hit(head: boolean) { this.beep(head ? 1400 : 900, 0.06, 0.12); }
  empty() { this.beep(300, 0.04, 0.1); }
  reload() { this.beep(500, 0.05, 0.08); setTimeout(() => this.beep(700, 0.05, 0.08), 180); }
  pickup() { this.beep(660, 0.07, 0.08, 'triangle'); }
  hurt() { this.beep(120, 0.15, 0.2, 'sawtooth'); }
}
