from __future__ import annotations

import unittest
from datetime import datetime, timezone

from app.local_tools import direct_success_text, plan_local_tool


class LocalToolPlannerTests(unittest.TestCase):
    def test_english_relative_reminder(self) -> None:
        plan = plan_local_tool("remind me in 20 minutes to stretch")
        self.assertIsNotNone(plan)
        assert plan is not None
        self.assertEqual(plan["tool"], "create_reminder")
        self.assertEqual(plan["args"]["text"], "stretch")
        due = datetime.fromisoformat(plan["args"]["due_at"].replace("Z", "+00:00"))
        self.assertGreater(due, datetime.now(timezone.utc))
        self.assertFalse(plan["needs_backend"])

    def test_chinese_relative_reminder(self) -> None:
        plan = plan_local_tool("20分钟后提醒我喝水")
        self.assertIsNotNone(plan)
        assert plan is not None
        self.assertEqual(plan["tool"], "create_reminder")
        self.assertEqual(plan["args"]["text"], "喝水")

    def test_explicit_iso_reminder(self) -> None:
        plan = plan_local_tool("remind me 2030-01-02T03:04:05Z to review the experiment")
        self.assertIsNotNone(plan)
        assert plan is not None
        self.assertEqual(plan["tool"], "create_reminder")
        self.assertEqual(plan["args"]["due_at"], "2030-01-02T03:04:05Z")
        self.assertEqual(plan["args"]["text"], "review the experiment")

    def test_list_reminders(self) -> None:
        plan = plan_local_tool("show my reminders")
        self.assertIsNotNone(plan)
        assert plan is not None
        self.assertEqual(plan["tool"], "list_reminders")
        self.assertTrue(plan["needs_backend"])

    def test_cancel_reminder_requires_explicit_id(self) -> None:
        reminder_id = "12345678-1234-1234-1234-123456789abc"
        plan = plan_local_tool(f"cancel reminder {reminder_id}")
        self.assertIsNotNone(plan)
        assert plan is not None
        self.assertEqual(plan["tool"], "cancel_reminder")
        self.assertEqual(plan["args"]["id"], reminder_id)

    def test_ordinary_chat_does_not_become_tool_request(self) -> None:
        self.assertIsNone(plan_local_tool("I forgot what I was going to do today"))
        self.assertIsNone(plan_local_tool("tell me a joke about reminders"))

    def test_direct_reminder_success_copy(self) -> None:
        result = {"due_at": "2030-01-02T03:04:05Z"}
        self.assertIn("2030-01-02", direct_success_text("create_reminder", result, chinese=False))
        self.assertIn("2030-01-02", direct_success_text("create_reminder", result, chinese=True))


if __name__ == "__main__":
    unittest.main()
