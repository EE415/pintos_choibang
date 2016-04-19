export testname='args-none'
pintos -v -k -T 60 --qemu  --fs-disk=2 -p build/tests/userprog/${testname} -a args-none -- -q   -f run ${testname}
