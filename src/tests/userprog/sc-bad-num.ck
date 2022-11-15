# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(sc-bad-num) begin
sc-bad-num: exit(-1)
EOF
pass;
