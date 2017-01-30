@echo off
ctime -begin build.ctm
pushd ..
call buildsuper 4tld\4tld_custom.cpp
popd
ctime -end build.ctm