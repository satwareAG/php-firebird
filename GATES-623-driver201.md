# Gates: driver#201 ext-side fix - commit_ret on ended explicit transaction self-heals

OWNS: fbird_transaction.c, tests/driver201_commit_ret_after_handle_end.phpt

- [x] G1: RED test turns GREEN - commit_ret on ended-but-alive explicit tx restarts transparently (php84-fb3-dev)
  CHECK: bash scripts/test_matrix.sh php84-fb3-dev "" tests/driver201_commit_ret_after_handle_end.phpt > /tmp/opencode/g1.log 2>&1; echo "matrix-exit:$?"
  EXPECT: matrix-exit:0
  EVIDENCE: exit=0; shell=/bin/sh; cwd=/home/mw/external/php-firebird; path=4f1febc988dd/17 entries; EXPECT=matched; gate-sig=9de438879dde0143; output-sha256=ca0ec61710c0759daf167c459ba2a01bb79c81947c64ebfaa1d66d0cdcf31f31; output-bytes=14

- [x] G2: no regression in existing commit_ret / transaction lifecycle tests (php84-fb3-dev)
  CHECK: bash scripts/test_matrix.sh php84-fb3-dev "" tests/fbird_commit_ret_001.phpt tests/fbird_commit_ret_lifecycle.phpt tests/fbird_trans_008.phpt tests/ddl_commit_ret_locks_001.phpt tests/ddl_commit_ret_locks_002.phpt tests/ddl_commit_ret_locks_003.phpt tests/issue589_rollback_ret_locks.phpt tests/issue540_metadata_lock_release.phpt tests/issue566_release_metadata_locks.phpt > /tmp/opencode/g2.log 2>&1; echo "matrix-exit:$?"
  EXPECT: matrix-exit:0
  EVIDENCE: exit=0; shell=/bin/sh; cwd=/home/mw/external/php-firebird; path=4f1febc988dd/17 entries; EXPECT=matched; gate-sig=5273c4c79f3b7b14; output-sha256=ca0ec61710c0759daf167c459ba2a01bb79c81947c64ebfaa1d66d0cdcf31f31; output-bytes=14

- [x] G3: touched test memory-safe under AddressSanitizer (php83-asan)
  CHECK: bash scripts/test_matrix.sh php83-asan "" tests/driver201_commit_ret_after_handle_end.phpt > /tmp/opencode/g3.log 2>&1; echo "matrix-exit:$?"
  EXPECT: matrix-exit:0
  EVIDENCE: exit=0; shell=/bin/sh; cwd=/home/mw/external/php-firebird; path=4f1febc988dd/17 entries; EXPECT=matched; gate-sig=5ef4a09c3c9514ed; output-sha256=ca0ec61710c0759daf167c459ba2a01bb79c81947c64ebfaa1d66d0cdcf31f31; output-bytes=14

- [x] G4: full local test matrix green (12 containers, PHP 8.2-8.5 x FB3/FB4/FB5)
  CHECK: bash scripts/test_matrix.sh > /tmp/opencode/g4.log 2>&1; echo "matrix-exit:$?"
  EXPECT: matrix-exit:0
  EVIDENCE: exit=0; shell=/bin/sh; cwd=/home/mw/external/php-firebird; path=4f1febc988dd/17 entries; EXPECT=matched; gate-sig=2fc15c46ad6add7d; output-sha256=ca0ec61710c0759daf167c459ba2a01bb79c81947c64ebfaa1d66d0cdcf31f31; output-bytes=14

- [x] G5: triage posted downstream - evidence-tagged classification comment on driver#201 + cross-link on php-firebird#621
  EVIDENCE: 2026-09-15 driver201#issuecomment-5688965267; php-firebird#621#issuecomment-5688967206
