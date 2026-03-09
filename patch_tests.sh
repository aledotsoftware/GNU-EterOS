#!/bin/bash
sed -i '1i #include <stdlib.h>' tests/test_rtc.c
sed -i 's/#define assert/#include <stdlib.h>\n#define assert/g' tests/test_rtc.c
