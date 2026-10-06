import { Sfx } from './game/audio';
import { Game, MatchResult } from './game/game';
import { Input } from './game/input';

const $ = (id: string) => document.getElementById(id)!;
const canvas = $('game') as HTMLCanvasElement;
const input = new Input(canvas);
const sfx = new Sfx();
let game: Game | null = null;

$('menu-hint').textContent = input.isTouch
  ? 'چپ: حرکت · راست: چرخاندن دوربین · 🔥 شلیک · 🎯 نشانه‌گیری · ✋ برداشتن'
  : 'برای کنترل دوربین روی صفحه کلیک کنید. با Esc موس آزاد می‌شود.';

async function enterFullscreen() {
  if (!input.isTouch) return;
  try {
    await document.documentElement.requestFullscreen?.();
    await (screen.orientation as unknown as { lock?: (o: string) => Promise<void> }).lock?.('landscape');
  } catch { /* not supported on every browser (e.g. iPhone Safari) */ }
}

function startMatch() {
  sfx.unlock();
  void enterFullscreen();
  game?.dispose();
  game = new Game(canvas, input, sfx, showEnd);
  $('menu').classList.add('hidden');
  $('end').classList.add('hidden');
  $('hud').classList.remove('hidden');
  $('touch-ui').classList.toggle('hidden', !input.isTouch);
  input.requestLock();
  game.start();
}

function showEnd(r: MatchResult) {
  document.exitPointerLock?.();
  $('touch-ui').classList.add('hidden');
  $('end-title').textContent = r.win ? '🏆 برنده شدی! آخرین بازمانده تو هستی' : 'کشته شدی';
  $('end-stats').textContent = `رتبه: #${r.place} · کشته‌ها: ${r.kills}`;
  $('end').classList.remove('hidden');
}

$('btn-start').addEventListener('click', startMatch);
$('btn-restart').addEventListener('click', startMatch);
