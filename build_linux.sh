gcc -I/usr/include/lua5.1 -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"src/xLuaAerospikeLayer.d" -MT"src/xLuaAerospikeLayer.d" -o "./src/xLuaAerospikeLayer.o" "src/xLuaAerospikeLayer.c" -std=gnu99 -g -rdynamic -Wall -fno-common -fno-strict-aliasing -fPIC -DMARCH_x86_64 -D_FILE_OFFSET_BITS=64 -D_REENTRANT -D_GNU_SOURCE
gcc -shared -o "xLuaAerospikeLayer.so"  ./src/xLuaAerospikeLayer.o -L/usr/lib -lm -llua5.1 -laerospike -lpthread -lssl -lrt -lev -lz
echo -lgnutls-openssl
cp -f ./xLuaAerospikeLayer.so /usr/lib/lua/5.1/xLuaAerospikeLayer.so
ls -lh /usr/lib/lua/5.1/xLuaAerospikeLayer.so