#!/usr/bin/env python3
"""Source regression checks: testing god mode is a hittable HP-locked target."""
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


def function_body(path, signature):
    text = (ROOT / path).read_text(encoding="utf-8")
    offset = text.index("{", text.index(signature))
    depth = 1
    end = offset + 1
    while depth:
        if text[end] == "{":
            depth += 1
        elif text[end] == "}":
            depth -= 1
        end += 1
    return text[offset + 1:end - 1]


class HittableGodContractTests(unittest.TestCase):
    def test_god_is_not_removed_from_eligible_targets(self):
        body = function_body("src/game/server/gamemodes/huntern.cpp", "bool CGameControllerHunterN::CanCombatInteract")
        self.assertNotIn("m_AsylumTestGod", body)
        self.assertIn("IsFriendlyFire", body)  # Keep ordinary team/mode restrictions.

    def test_damage_keeps_the_hit_pipeline_and_only_locks_vitals(self):
        body = function_body("src/game/server/entities/character.cpp", "bool CCharacter::TakeDamage")
        self.assertNotRegex(body, r"if\(m_pPlayer->m_AsylumTestGod\)\s*return")
        self.assertIn("DamageFlag |= DAMAGE_NO_DAMAGE | DAMAGE_NO_DEATH;", body)
        self.assertLess(body.index("Controller()->OnCharacterTakeDamage"), body.index("m_AsylumTestGod"))
        self.assertIn("DAMAGE_NO_KNOCKBACK", body)
        self.assertIn("SOUND_HIT", body)
        self.assertIn("CreateDamageInd", body)
        self.assertIn("EMOTE_PAIN", body)
        self.assertIn("if(!(DamageFlag & DAMAGE_NO_DAMAGE))", body)
        self.assertEqual(body.count("m_AsylumTestGod"), 1)

    def test_freeze_and_deep_freeze_are_not_disabled_by_god(self):
        for signature in ["bool CCharacter::Freeze(", "bool CCharacter::DeepFreeze("]:
            self.assertNotIn("m_AsylumTestGod", function_body("src/game/server/entities/character.cpp", signature))

    def test_enable_does_not_unfreeze_and_removes_spawn_shield(self):
        body = function_body("src/game/server/gamemodes/huntern.cpp", "void CGameControllerHunterN::ConTestGod")
        self.assertIn("Protect(0.0f, false)", body)
        self.assertNotIn("UnFreeze()", body)
        self.assertNotIn("UndeepFreeze()", body)
        spawn = function_body("src/game/server/gamemodes/huntern.cpp", "void CGameControllerHunterN::OnCharacterSpawn")
        self.assertIn("m_AsylumTestGod ? 0.0f", spawn)

    def test_weapon_debuffs_do_not_require_lost_hp_for_god(self):
        laser = function_body("src/game/server/weapons/asylum.cpp", "bool CAsylumWeapon::LaserHit")
        self.assertIn("|| pHit->GetPlayer()->m_AsylumTestGod", laser)
        self.assertIn("!pHit->IsProtected()", laser)
        gods = (ROOT / "src/game/server/weapons/asylum_god.cpp").read_text(encoding="utf-8")
        self.assertIn("|| pTarget->GetPlayer()->m_AsylumTestGod", gods)
        self.assertIn("|| GodTargetHit", gods)
        self.assertRegex(gods, r"GodTargetHit\s*=.*m_AsylumTestGod\s*&&\s*Hunter\(pProj\)->CanWeaponInteract")


if __name__ == "__main__":
    unittest.main()
