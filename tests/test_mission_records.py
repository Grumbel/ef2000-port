# SPDX-FileCopyrightText: 2026 OpenAI <codex@openai.com>
# SPDX-License-Identifier: GPL-3.0-or-later

"""Asset-free fixtures for EF2000's human-readable scenario data."""

import unittest

from tools.mission_records import (Record, block_json, group_records,
                                   inspect_bytes, inspect_records, parse_text)


MISSIONS = b'''\
; scenario fixture, not original data\r\n
NUM_MISSIONS 2 INCLUDE_PILOTS\r\n
WORLD_STATUS 0 0 0 0\r\n
MISSION PLANE NATO INTC OK 0 0 1\r\n
FLIGHT 2 EFA INTERCEPT OK\r\n
PYLON 0 AMRAAM\r\n
WP_ROUTE 2 0 0\r\n
WAYPOINT 0 1 2 3 4 5 PATROL 0 0 0\r\n
WAYPOINT 1 2 3 4 5 6 LANDING 0 0 0\r\n
TARGET MISSION 1 GROUP 0\r\n
PIL "CALL SIGN" 0 1 2\r\n
MISSION GBASED NATO AIR_DEFENSE OK -1 1 1\r\n
GROUP 1 RADAR INTERCEPT OK\r\n
ATTACK_ROUTE 1\r\n
ATTACK_NODE 1 2 3 4 5 6 PASS\r\n
NUM_BASES 1\r\n
BASE NATO AIRFIELD "Some Base" 0 0 0.0 0\r\n
TIMESLOTS 2\r\n
TIMESLOT 0 0 0 TAKE_OFF CAP 0 0\r\n
TIMESLOT 0 0 1 LANDING CAP 0 0\r\n
'''

NAVIGATION = b'''\
; navigation fixture\r\n
NUM_BASE_NAVIGATIONS 2\r\n
BASE_NAVIGATION AIRFIELD 4 0 1\r\n
3D_POSITION 0 0 0 0 0 0\r\n
NAVIG_CORRIDOR 0 1 0 0\r\n
NAVIG_ROUTE 2\r\n
WAYPOINT 0 0 0 100 0 0 PATROL 2 1995 -1\r\n
WAYPOINT 1 1 1 100 0 0 PATROL 2 1995 -1\r\n
BASE_NAVIGATION AIRFIELD 6 0 2\r\n
3D_POSITION 0 0 0 0 0 0\r\n
NAVIG_CORRIDOR 0 1 0 0\r\n
NAVIG_ROUTE 1\r\n
WAYPOINT 0 0 0 100 0 0 PATROL 2 1995 -1\r\n
NAVIG_CORRIDOR 1 1 0 0\r\n
NAVIG_ROUTE 1\r\n
WAYPOINT 0 1 1 100 0 0 PATROL 2 1995 -1\r\n
'''

CAMPAIGN = b'''\
NUM_TARGET_AREAS 2\r\n
SSNAME "Air Base One" NATO 1 2 3 ALL\r\n
SSNAME "Air Base Two" SWEDISH 1 2 3 ALL\r\n
NUM_MOBILES 2\r\n
MOB PL EFA OK NATO "CALL SIGN" 0 100 0 1 0 -1 -1 -1 0\r\n
PIL "CALL SIGN" 0 0 0 0\r\n
MOB PL EFA OK NATO "CALL 2" 1 100 0 1 0 -1 -1 -1 0\r\n
PIL "CALL 2" 0 0 0 0\r\n
'''


class MissionRecordsTests(unittest.TestCase):
    def test_mission_routes_base_slots_and_grouping(self):
        report, records = inspect_bytes(MISSIONS)
        self.assertEqual(report['kind'], 'mission')
        self.assertEqual(report['problems'], [])
        self.assertEqual(report['declarations'], {'NUM_MISSIONS': 2, 'NUM_BASES': 1})
        self.assertEqual(report['routes']['WP_ROUTE']['children'], 2)
        self.assertEqual(report['routes']['ATTACK_ROUTE']['children'], 1)
        self.assertEqual(report['routes']['TIMESLOTS']['children'], 2)
        self.assertEqual(report['block_checks']['mission_header_ordinal']['mismatches'], [])
        # The fixture deliberately has only one pylon; this inferred
        # relationship is reported but doesn't invalidate a count-correct file.
        self.assertEqual(len(report['block_checks']['mission_flight_pylons']['mismatches']), 1)
        headers, blocks = group_records(records, report['kind'])
        self.assertEqual([block['kind'] for block in blocks], ['MISSION', 'MISSION', 'BASE'])
        self.assertEqual([block['index'] for block in blocks], [0, 1, 0])
        self.assertIn('NUM_BASES', [record.keyword for record in headers])
        self.assertEqual(block_json(blocks[0])['directives'][-1]['args'][0], 'CALL SIGN')
        self.assertEqual(blocks[-1]['header'].args[2], 'Some Base')

    def test_navigation_corridors_and_waypoint_indexes(self):
        report, records = inspect_bytes(NAVIGATION)
        self.assertEqual(report['problems'], [])
        self.assertEqual(report['declarations']['NUM_BASE_NAVIGATIONS'], 2)
        self.assertEqual(report['routes']['NAVIG_ROUTE']['count'], 3)
        self.assertEqual(report['routes']['NAVIG_ROUTE']['children'], 4)
        self.assertEqual(report['waypoint_index_details'], [])
        self.assertEqual(report['block_checks']['navigation_position_and_corridors']['mismatches'], [])
        self.assertEqual(len(group_records(records, report['kind'])[1]), 2)

    def test_campaign_mobiles_and_quoted_callsigns(self):
        report, records = inspect_bytes(CAMPAIGN)
        self.assertEqual(report['problems'], [])
        self.assertEqual(report['declarations']['NUM_TARGET_AREAS'], 2)
        self.assertEqual(report['declarations']['NUM_MOBILES'], 2)
        self.assertEqual([b['kind'] for b in group_records(records, report['kind'])[1]],
                         ['SSNAME', 'SSNAME', 'MOB', 'MOB'])
        self.assertEqual(report['block_checks']['mobile_header_ordinal']['mismatches'], [])
        self.assertEqual(report['block_checks']['mobile_pilot_record']['mismatches'], [])
        self.assertEqual(records[1].args[0], 'Air Base One')

    def test_missing_route_child_is_reported_not_discarded(self):
        data = b'NUM_BASE_NAVIGATIONS 1\nBASE_NAVIGATION AIRFIELD 0 0 1\nNAVIG_CORRIDOR 0\nNAVIG_ROUTE 2\nWAYPOINT 0\n'
        report, _ = inspect_bytes(data)
        self.assertTrue(any('declares 2 WAYPOINT; observed 1' in p
                            for p in report['problems']))

    def test_detects_corridor_and_declaration_mismatch(self):
        data = b'NUM_BASE_NAVIGATIONS 2\nBASE_NAVIGATION AIRFIELD 0 0 2\nNAVIG_CORRIDOR 0\n'
        report, _ = inspect_bytes(data)
        self.assertEqual(len(report['problems']), 2)

    def test_unusual_waypoint_index_is_reported_without_rewriting_it(self):
        data = b'NUM_BASE_NAVIGATIONS 1\nBASE_NAVIGATION AIRFIELD 0 0 0\nNAVIG_ROUTE 1\nWAYPOINT 9\n'
        report, records = inspect_bytes(data)
        self.assertEqual(report['problems'], [])
        self.assertEqual(report['waypoint_index_details'][0]['actual_index'], 9)
        self.assertEqual(records[-1].raw, 'WAYPOINT 9')

    def test_original_loader_rejects_zero_route_and_invalid_last_waypoint(self):
        # Constraints recovered from guest 0x0000f314; the payload reader
        # explicitly tests the route count and its last-waypoint index.
        zero, _ = inspect_bytes(b'NUM_MISSIONS 0\nWP_ROUTE 0 0 0\n')
        self.assertTrue(any('at least one waypoint' in item
                            for item in zero['problems']))
        beyond, _ = inspect_bytes(
            b'NUM_MISSIONS 0\nWP_ROUTE 2 0 2\nWAYPOINT 0\nWAYPOINT 1\n')
        self.assertTrue(any('last waypoint 2 outside' in item
                            for item in beyond['problems']))
        valid, _ = inspect_bytes(
            b'NUM_MISSIONS 0\nWP_ROUTE 2 0 1\nWAYPOINT 0\nWAYPOINT 1\n')
        self.assertEqual(valid['problems'], [])

    def test_nested_attack_pattern_route_and_node_counts(self):
        # 0x0000f08c -> 0x0000f038 -> 0x0000efe8, with nested variable
        # length routes. No original assets are needed to validate the shape.
        data = (b'ATTACK_PATTERN LAYDOWN_ATTACK 2\n'
                b'ATTACK_ROUTE 1\n'
                b'ATTACK_NODE 0 0 700 0 0 0 PASS\n'
                b'ATTACK_ROUTE 2\n'
                b'ATTACK_NODE 0 0 700 0 0 0 STRIKE\n'
                b'ATTACK_NODE 0 0 700 0 0 0 PASS\n')
        report, _ = inspect_bytes(data)
        self.assertEqual(report['problems'], [])
        self.assertEqual(report['attack_patterns'], {'count': 1, 'routes': 2, 'nodes': 3})
        self.assertEqual(report['routes']['ATTACK_ROUTE']['children'], 3)

        wrong, _ = inspect_bytes(data.replace(b'LAYDOWN_ATTACK 2', b'LAYDOWN_ATTACK 3'))
        self.assertTrue(any('ATTACK_PATTERN declares 3 ATTACK_ROUTE; observed 2' in issue
                            for issue in wrong['problems']))
        short, _ = inspect_bytes(data.replace(b'ATTACK_ROUTE 2', b'ATTACK_ROUTE 4'))
        self.assertTrue(any('ATTACK_ROUTE declares 4 ATTACK_NODE; observed 2' in issue
                            for issue in short['problems']))

    def test_truncated_quoted_field_and_bad_counts_fail(self):
        with self.assertRaisesRegex(ValueError, 'line 1'):
            parse_text('MOB "unterminated')
        with self.assertRaisesRegex(ValueError, 'noninteger'):
            inspect_records([Record(1, 'NUM_MISSIONS', ('n/a',), 'NUM_MISSIONS n/a')])

    def test_inline_semicolon_is_not_a_comment_in_original_line_reader(self):
        records = parse_text('; whole-line comment\nDIRECTIVE value ;extra\n')
        self.assertEqual(len(records), 1)
        self.assertEqual(records[0].args, ('value', ';extra'))


if __name__ == '__main__':
    unittest.main()
