This is a small project to do some checking on c codes,mainly using clang's libclang and ASTMatcher to do the task.
It runs on Ubuntu 12.04.

--------------------------Setting---------------------------------------------------
first, you should set the path of llvm's lib in the 'makefile'
then, set the path for clang to search for headers in 'config.ini'
just make it
copy your test folder in CopyHeaderFile
sh fileBash.sh <test folder name> <test.c>
cd CopyHeadFile/<test folder name>
make
run your application like 'sh run.sh'
./matchCheck
you will get 'checkLog1.txt' and 'checkLog2.txt' containing the result

--------------------------Example---------------------------------------------------

when you are in the directory of 'clangCheckingTool'
cp -r otherTest CopyHeadFile/otherTest
sh fileBash.sh otherTest otherTest.c
cd CopyHeadFile/otherTest
make
sh runme.sh