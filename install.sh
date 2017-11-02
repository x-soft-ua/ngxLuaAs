#!/bin/sh
service nginx stop
tar -xvzf ./nginx-1.13.6.tar.gz
cd ./nginx-1.13.6
make clean
apt-get -y install libexpat1-dev libexpat1-dev libpcre++-dev gcc libgeoip-dev
apt-get -y install openssl=1.0.1f-1ubuntu2.22
apt-get -y install libssl-dev=1.0.1f-1ubuntu2.22
./configure --with-cc-opt='-g -O2 -fPIE -fstack-protector --param=ssp-buffer-size=4 -Wformat -Werror=format-security -fPIC -D_FORTIFY_SOURCE=2' --with-ld-opt='-Wl,-Bsymbolic-functions -fPIE -pie -Wl,-z,relro -Wl,-z,now -fPIC' --prefix=/usr/share/nginx --conf-path=/etc/nginx/nginx.conf --http-log-path=/var/log/nginx/access.log --error-log-path=/var/log/nginx/error.log --lock-path=/var/lock/nginx.lock --pid-path=/run/nginx.pid --modules-path=/usr/lib/nginx/modules --http-client-body-temp-path=/var/lib/nginx/body --http-fastcgi-temp-path=/var/lib/nginx/fastcgi --http-proxy-temp-path=/var/lib/nginx/proxy --http-scgi-temp-path=/var/lib/nginx/scgi --http-uwsgi-temp-path=/var/lib/nginx/uwsgi --with-debug --with-pcre-jit --with-ipv6 --add-module=nginx-sla --with-http_dav_module --add-module=nginx-dav-ext-module-0.1.0 --add-module=ngx_devel_kit-0.3.0 --add-module=lua-nginx-module-0.10.11rc3 --add-module=echo-nginx-module-0.61 --with-http_geoip_module --with-http_ssl_module --with-http_realip_module --with-http_addition_module --with-http_sub_module --with-http_dav_module --with-http_flv_module --with-http_mp4_module --with-http_gunzip_module --with-http_gzip_static_module --with-http_random_index_module --with-http_secure_link_module --with-http_stub_status_module --with-http_auth_request_module --with-threads --with-stream --with-stream_ssl_module --with-http_slice_module --with-mail --with-mail_ssl_module --with-file-aio --with-http_v2_module
make -j2
make install
mv /usr/share/nginx/sbin/nginx /usr/sbin/nginx
apt-get -y install liblua5.1-0-dev
apt-get -y install libev-libevent-dev
cd ..
dpkg -i ./aerospike-client-c-libev-devel-4.2.0.ubuntu14.04.x86_64.deb
mkdir -p /usr/lib/lua/5.1/
apt-get install gcc libssl-dev
chmod +x ./build_linux.sh; ./build_linux.sh
cp /data/www/current/lua/lib/c-modules/xLuaAerospike/xLuaAerospikeLayer.so /usr/lib/lua/5.1/
service nginx start
