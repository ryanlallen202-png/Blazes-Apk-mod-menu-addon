"""Source-level regression checks; not a substitute for the NDK build/headset tests."""
from pathlib import Path
import unittest

SOURCE = (Path(__file__).resolve().parents[1] / "src/menu.cpp").read_text()

class SafetyInvariants(unittest.TestCase):
    def test_global_send_budget(self):
        self.assertIn('g_netSends < 2 && !g_itemQueue.empty()', SOURCE)
        self.assertIn('g_netSends >= 2', SOURCE)
        self.assertIn('static void OnUpdate(float dt) {\n    g_netSends = 0;', SOURCE)

    def test_queue_caps(self):
        self.assertIn('list.size() > 27', SOURCE)
        self.assertIn('g_itemQueue.size() + list.size() > 32', SOURCE)
        self.assertIn('g_itemQueue.size() >= 32', SOURCE)
        self.assertIn('g_nextItem = g_time + 0.3', SOURCE)
        self.assertIn('g_nextBatch = g_time + 3.0', SOURCE)
        self.assertIn('g_nextSharedEffect = g_time + 1.0', SOURCE)

    def test_explicit_safe_item(self):
        self.assertIn('selected != "balloon"', SOURCE)
        self.assertIn('if (!g_itemQueue.empty())', SOURCE)
        self.assertIn('i < n && i < 512', SOURCE)

    def test_home_and_cleanup(self):
        self.assertIn('if (hit == -3)', SOURCE)
        self.assertIn('g_cat = MAIN; g_page = 0; g_dirty = true;', SOURCE)
        room = SOURCE.split('static void OnRoomChange() {', 1)[1].split('static void OnUpdate', 1)[0]
        self.assertIn('PauseTrackers(false)', room)
        self.assertIn('g_itemQueue.clear()', room)
        self.assertIn('g_tagPlayer.clear()', room)

    def test_tech_reader_has_no_refund(self):
        self.assertNotIn('TryRefund', SOURCE)
        self.assertIn('G.techPattern[bubble]()', SOURCE)
        self.assertIn('ym::onUpdate(SafeUpdate)', SOURCE)

if __name__ == '__main__':
    unittest.main()
