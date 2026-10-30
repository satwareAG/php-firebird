# PR checklist

- [ ] Tests: `.phpt`/PHPUnit added or updated for every behavior change
- [ ] If this PR changes `zend_parse_parameters` defaults, argument forwarding, or DPB/SPB/TPB buffer building (#681): a wire/engine-observation test (MON$ columns, header state, service output) rides this same PR - not only return-value assertions. CI runs `scripts/gates/issue-681-zpp-wire-gate.sh`.
- [ ] For every parsed argument in touched zpp blocks: grep the call chain and confirm it is FORWARDED to the implementation (parsed-but-unforwarded = dead parameter, blocks merge; #595/#671 family)
- [ ] Connect-path argument changes reviewed by someone other than the author
- [ ] `bash scripts/test_matrix.sh` green (or CI will be)
