# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF', <<'EOF']);
(exec-over-arg) begin
(exec-over-arg) exec() did not panic the kernel and returned: -1
(exec-over-arg) end
exec-over-arg: exit(0)
EOF
(exec-over-arg) begin
(exec-over-arg) exec() did not panic the kernel and returned: 0
(exec-over-arg) end
exec-over-arg: exit(0)
EOF
pass;
