# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF',<<'EOF']);
(exec-over-arg) begin
(exec-over-arg) exec() did not panic the kernel
(exec-over-arg) end
exec-over-arg: exit(0)
EOF
(exec-over-arg) begin
child-args: exit(-1)
(exec-over-arg) exec() did not panic the kernel
(exec-over-arg) end
exec-over-arg: exit(0)
EOF
pass;
