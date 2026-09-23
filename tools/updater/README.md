# WoW335Updater 0.1.0-335 — experimental port
Windows x86 WinForms external updater, bound ONLY to github12wykrzyk/335. Does not inject DLLs.
TEST(work): newest successful Build 335 work candidate; STABLE(main): newest successful Build 335 stable candidate. Requires actual final-gated runtime artifacts; empty bootstrap has none.
Validates package SHA256 from candidate_metadata.json before installing, keeps managed-file backups, supports rollback, verify/repair, update+play, updater self-update, optional sanitized issue reports. Token for public repo is optional for read operations if API rate limits allow; a report token grants Issues write only. Never commit tokens.
The 1.12 realmlist selector is DISABLED pending localization-aware WoW 3.3.5a path support. Do not alter existing Data/<locale>/realmlist.wtf.
The compiled updater is not yet proof of correct game integration or tested in-game; require exact 12340 in-game test before stable promotion.
