# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(exec-over-args) begin
(exec-over-args) exec() did not panic the kernel
(exec-over-args) end
exec-over-args: exit(0)
EOF
pass;
