import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from app import AutoFocus


class FakeMotor:
    def __init__(self, pos):
        self.current_pos = self.target_pos = pos
        self.running = False
        self.speed = 250
        self.targets = []

    def set_target(self, pos):
        self.target_pos = self.current_pos = pos
        self.targets.append(pos)

    def stop(self):
        pass


def run(metric):
    m = FakeMotor(100)
    af = AutoFocus(m, lambda roi, mode, frames: metric(m.current_pos))
    af.start({'mode': 'day', 'range': 200, 'step': 50, 'backlash': 20, 'settle_ms': 0, 'frames': 1})
    while af.status()['running']:
        pass
    return m, af.status()


m, s = run(lambda pos: {'ok': True, 'score': 100 - (pos - 123) ** 2 / 100})
assert s['phase'] == 'done', s
assert abs(s['best_pos'] - 123) <= 5, s
assert len(s['points']) == 9, s
assert m.targets[-2] < m.targets[-1], m.targets

m, s = run(lambda pos: {'ok': True, 'score': 0.0})
assert s['phase'] == 'error', s
assert m.targets[-1] == 100, m.targets

try:
    AutoFocus(FakeMotor(0), None).start({'mode': 'x'})
    raise SystemExit('expected ValueError')
except ValueError:
    pass
print('OK')
