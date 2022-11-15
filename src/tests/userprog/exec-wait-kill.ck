# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF',<<'EOF',<<'EOF']);
(exec-wait-kill) begin
(exec-wait-kill) exec() = 0
(child-bad) begin
child-bad: exit(-1)
(exec-wait-kill) wait() = -1
(exec-wait-kill) end
exec-wait-kill: exit(0)
EOF
(exec-wait-kill) begin
(child-bad) begin
(exec-wait-kill) exec() = 0
child-bad: exit(-1)
(exec-wait-kill) wait() = -1
(exec-wait-kill) end
exec-wait-kill: exit(0)
EOF
(exec-wait-kill) begin
(child-bad) begin
child-bad: exit(-1)
(exec-wait-kill) exec() = 0
(exec-wait-kill) wait() = -1
(exec-wait-kill) end
exec-wait-kill: exit(0)
EOF
pass;
