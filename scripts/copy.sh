DIR_ROOT=/home/fky/code/git/mine/Server

cd `pwd`
mkdir -p ${DIR_ROOT}/include/feverrpc ${DIR_ROOT}/src
cp include/feverrpc/{feverrpc,threadmanager}.hpp ${DIR_ROOT}/include/feverrpc/
cp src/{feverrpc,threadmanager}.cpp ${DIR_ROOT}/src