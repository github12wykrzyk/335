# Restored responsive AutoLoot335.dll TEST provenance

This is the original exact-byte work module, not a recompiled frame experiment:
source branch work at 3cbc14b66aa2ed5687b0132190c4381494c0bdb6,
Windows x86 native CI run 35872640642, Git blob
9169f0874e470ce030ef4727f6120a04059eab30, SHA256
6551b34fde100edaf0b9597b4e267d1844acf92da379fae344c41efeaa8c31c3.
The user reported that native-frame AutoLoot stops looting entirely, while
the prior variant looted when stationary. This restores the old managed
module and matching message-hook loader in TEST only, without modifying
work or STABLE. During sustained movement even native manual loot may fail;
do not claim this rollback makes movement looting possible.
