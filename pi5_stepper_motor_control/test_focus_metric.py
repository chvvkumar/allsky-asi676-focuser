import sys, types, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.modules['zwoasi'] = types.SimpleNamespace()
_fc = types.ModuleType('flask_cors')
_fc.CORS = lambda app: None
sys.modules['flask_cors'] = _fc
import numpy as np
import cv2
import zwo

def make_image():
    rng = np.random.default_rng(1)
    yy, xx = np.mgrid[0:200, 0:200]
    img = np.full((200, 200), 20.0)
    for cx, cy in [(30 + 35 * (i % 5), 50 + 80 * (i // 5) + 7 * i) for i in range(10)]:
        img += 150 * np.exp(-((xx - cx) ** 2 + (yy - cy) ** 2) / (2 * 1.2 ** 2))
    img += rng.normal(0, 3, img.shape)
    return np.clip(img, 0, 255).astype(np.uint8)

if __name__ == "__main__":
    sharp = make_image()
    blur = cv2.GaussianBlur(sharp, (0, 0), 3)
    s, stars, hfr = zwo.night_score(sharp)
    sb, _, _ = zwo.night_score(blur)
    assert stars >= 5, stars
    assert sb < s, (sb, s)
    hfrs = [zwo.night_score(cv2.GaussianBlur(sharp, (0, 0), b))[2] for b in (0.8, 1.5, 2.5, 3.5)]
    assert hfrs == sorted(hfrs), hfrs
    assert zwo.day_score(sharp) > zwo.day_score(blur)
    assert zwo.crop_roi(sharp, 0.25, 0.25, 0.5, 0.5).shape == (100, 100)
    print("OK")
