@echo off

:: LICENSE
:: 
:: This software is dual-licensed to the public domain and under the following
:: license: you are granted a perpetual, irrevocable license to copy, modify,
:: publish, and distribute this file as you see fit.

:: NOTE
:: This build script is used for compiling my full customization layer. Unless
:: you want my exact 4coder setup (colors, keybindings and all), #include the
:: drop-in command packs you want in your 4coder_custom.cpp instead.
:: 
:: No build steps will be required in addition to the default buildsuper.bat
:: script. For additional instruction, see the documentation comments at the
:: top of the respective files.

ctime -begin build.ctm
pushd ..
call buildsuper 4tld\4tld_custom.cpp
popd
ctime -end build.ctm