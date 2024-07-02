#https://github.com/jkeveren/jamesmon
cromple \
-g \
-std=c++20 \
-static \
-Wfatal-errors -Werror \
-Wall -Wextra -Wpedantic \
-Wfloat-equal -Wsign-conversion -Wfloat-conversion \
-Wno-error=unused-but-set-parameter \
-Wno-error=unused-but-set-variable \
-Wno-error=unused-function \
-Wno-error=unused-label \
-Wno-error=unused-local-typedefs \
-Wno-error=unused-parameter \
-Wno-error=unused-result \
-Wno-error=unused-variable \
-Wno-error=unused-value \
-o bin/jamesmon

# sudo setcap "cap_perfmon=pe" bin/jamesmon
