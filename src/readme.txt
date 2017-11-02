Packages:
    apt-get install liblua5.1-0-dev
    apt-get install libev-libevent-dev
    dpkg -i /data/www/current/lua/lib/c-modules/xLuaAerospike/aerospike-client-c-libev-devel-4.2.0.ubuntu14.04.x86_64.deb
    mkdir -p /usr/lib/lua/5.1/
    apt-get install gcc libssl-dev
    cd /data/www/current/lua/lib/c-modules/xLuaAerospike/; chmod +x ./build_linux.sh; ./build_linux.sh
    cp /data/www/current/lua/lib/c-modules/xLuaAerospike/xLuaAerospikeLayer.so /usr/lib/lua/5.1/

    nginx.conf:
        Add to http-section:
            lua_shared_dict sysmem 10M;
            init_by_lua 'ngx.shared.sysmem:set("worker_id", 0)';
            init_worker_by_lua_file /data/www/current/lua/ngxInitWorker.lua;

    localtions.include

        location /aero {
            add_header Content-type "text/plain";
            content_by_lua_file /data/www/current/lua/aero_match.lua;
        }

        ### phpd location, append to rewrite_by_lua ==>>
        # assert(loadfile("/data/www/current/lua/aero_rewrite.lua"))()

lua example:
    local json = require "json"
    local asKey = "new_test_key"

    local as_layer = require "xLuaAerospike"
    local as_connection = as_layer.as_conf_init("127.0.0.1", 3000, "test", "demo")

    local status = as_layer.as_bin_set(as_connection, asKey, "strval", ngx.md5(ngx.now()))
    if (status == as_layer.AS_LAYER_OK) then
        ngx.say("Set strval ok")
    end
                                                                                                                                                                                                               
    local rec, status = as_layer.as_get_record(as_connection, asKey)
    if (status == as_layer.AS_LAYER_OK) then
        ngx.say("Get record = " .. json:encode(rec))
    end